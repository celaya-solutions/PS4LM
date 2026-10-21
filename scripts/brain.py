#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["claude-agent-sdk>=0.2,<0.3"]
# ///
"""Bigger brain: an OpenAI-style chat endpoint on the Mac that hands a chat to
Claude or Codex, for when the model on the PS4 is too small.

Sign-in comes from the CLIs you already use, not API keys: Claude through the
Claude Agent SDK (your `claude` login), Codex through `codex exec` (your
`codex login`). Neither gets tools: Claude has none, Codex runs read-only in an
empty folder.

    uv run --script scripts/brain.py                         # 127.0.0.1:8090
    BRAIN_TOKEN=secret uv run --script scripts/brain.py --host 0.0.0.0

Models: "claude" and "codex" (each CLI's default model), or "claude:<model>" /
"codex:<model>". The system prompt is prompts/brain-system.txt; a client's own
system message goes after it.
"""
import argparse
import asyncio
import json
import os
import subprocess
import tempfile
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from claude_agent_sdk import ClaudeAgentOptions, ResultMessage, query

ROOT = Path(__file__).resolve().parent.parent
SYSTEM = (ROOT / "prompts" / "brain-system.txt").read_text().strip()
TOKEN = os.environ.get("BRAIN_TOKEN", "")
MODELS = ["claude", "codex"]


def _text(content) -> str:
    # OpenAI content is a string or a list of parts; only text parts count.
    if isinstance(content, str):
        return content
    return "".join(p.get("text", "") for p in content or [] if p.get("type") == "text")


def split_chat(messages: list[dict]) -> tuple[str, str]:
    """System text (ours first) and the chat as one transcript prompt."""
    system = "\n\n".join([SYSTEM] + [_text(m["content"]) for m in messages if m["role"] == "system"])
    turns = [m for m in messages if m["role"] in ("user", "assistant")]
    if len(turns) == 1:
        return system, _text(turns[0]["content"])
    lines = [f"{m['role'].capitalize()}: {_text(m['content'])}" for m in turns]
    return system, "The chat so far:\n\n" + "\n\n".join(lines) + "\n\nReply to the last user message."


async def _claude(system: str, prompt: str, model: str | None) -> str:
    opts = ClaudeAgentOptions(tools=[], system_prompt=system, max_turns=1,
                              setting_sources=[], model=model)
    result = None
    # Read to the end: leaving the SDK's generator early fails its cleanup ("aclose(): ... already running").
    async for msg in query(prompt=prompt, options=opts):
        if isinstance(msg, ResultMessage):
            result = msg
    if result is None:
        raise RuntimeError("Claude gave no result")
    if result.is_error:
        raise RuntimeError(result.result or result.subtype)
    return result.result or ""


def ask_claude(system: str, prompt: str, model: str | None) -> str:
    return asyncio.run(_claude(system, prompt, model))


def ask_codex(system: str, prompt: str, model: str | None) -> str:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "last.txt"
        # json.dumps of a str is a valid TOML basic string.
        cmd = ["codex", "exec", "--ephemeral", "--skip-git-repo-check", "--ignore-user-config",
               "--ignore-rules", "-s", "read-only", "-C", tmp, "-o", str(out),
               "-c", f"developer_instructions={json.dumps(system)}"]
        if model:
            cmd += ["-m", model]
        run = subprocess.run(cmd + ["-"], input=prompt, capture_output=True, text=True, timeout=600)
        if run.returncode != 0 or not out.exists():
            raise RuntimeError((run.stderr or run.stdout).strip()[-500:] or "codex failed")
        return out.read_text().strip()


def answer(model_id: str, messages: list[dict]) -> str:
    brain, _, model = model_id.partition(":")
    if brain not in MODELS:
        raise ValueError(f"unknown model {model_id!r}; use one of {MODELS}")
    system, prompt = split_chat(messages)
    return (ask_claude if brain == "claude" else ask_codex)(system, prompt, model or None)


class Handler(BaseHTTPRequestHandler):
    def _send(self, code: int, body: dict | None = None, raw: bytes = b"", ctype="application/json"):
        data = json.dumps(body).encode() if body is not None else raw
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _authed(self) -> bool:
        if not TOKEN or self.headers.get("Authorization", "") == f"Bearer {TOKEN}":
            return True
        self._send(401, {"error": {"message": "bad or missing bearer token"}})
        return False

    def do_GET(self):
        if self.path == "/health":
            return self._send(200, {"status": "ok"})
        if not self._authed():
            return
        if self.path == "/v1/models":
            return self._send(200, {"object": "list", "data": [
                {"id": m, "object": "model", "owned_by": "brain"} for m in MODELS]})
        self._send(404, {"error": {"message": "not found"}})

    def do_POST(self):
        if not self._authed():
            return
        if self.path != "/v1/chat/completions":
            return self._send(404, {"error": {"message": "not found"}})
        try:
            req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            model = req.get("model") or "claude"
            text = answer(model, req["messages"])
        except (ValueError, KeyError, TypeError) as e:
            return self._send(400, {"error": {"message": str(e)}})
        except Exception as e:  # the brain failed: not signed in, timeout, refusal
            return self._send(502, {"error": {"message": str(e)}})
        cid, now = f"chatcmpl-{uuid.uuid4().hex}", int(time.time())
        if not req.get("stream"):
            return self._send(200, {"id": cid, "object": "chat.completion", "created": now, "model": model,
                                    "choices": [{"index": 0, "finish_reason": "stop",
                                                 "message": {"role": "assistant", "content": text}}]})
        # shortcut: the whole reply goes in one chunk; stream token by token if a client needs it.
        chunk = {"id": cid, "object": "chat.completion.chunk", "created": now, "model": model}
        events = [dict(chunk, choices=[{"index": 0, "delta": {"role": "assistant", "content": text},
                                        "finish_reason": None}]),
                  dict(chunk, choices=[{"index": 0, "delta": {}, "finish_reason": "stop"}])]
        sse = "".join(f"data: {json.dumps(e)}\n\n" for e in events) + "data: [DONE]\n\n"
        self._send(200, raw=sse.encode(), ctype="text/event-stream")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8090)
    args = ap.parse_args()
    if args.host not in ("127.0.0.1", "localhost", "::1") and not TOKEN:
        raise SystemExit("set BRAIN_TOKEN before listening beyond this Mac: the brain spends your Claude and Codex plans")
    print(f"brain on http://{args.host}:{args.port}/v1 ({', '.join(MODELS)})", flush=True)
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
