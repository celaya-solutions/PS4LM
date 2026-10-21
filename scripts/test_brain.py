#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["claude-agent-sdk>=0.2,<0.3"]
# ///
"""Checks brain.py's token lock without calling Claude or Codex:

    uv run --script scripts/test_brain.py          # token lock only
    uv run --script scripts/test_brain.py --live   # also one real Claude call: no tools, no connectors
"""
import asyncio
import os
import sys
import threading
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

os.environ["BRAIN_TOKEN"] = "1234"  # brain.py reads it on import
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import brain  # noqa: E402

brain.Handler.log_message = lambda *args: None  # no access log lines
srv = ThreadingHTTPServer(("127.0.0.1", 0), brain.Handler)
threading.Thread(target=srv.serve_forever, daemon=True).start()


def status(token: str | None) -> int:
    headers = {"Authorization": f"Bearer {token}"} if token else {}
    req = urllib.request.Request(f"http://127.0.0.1:{srv.server_port}/v1/models", headers=headers)
    try:
        return urllib.request.urlopen(req).status
    except urllib.error.HTTPError as e:
        return e.code


assert status("1234") == 200
for _ in range(brain.MAX_BAD_TOKENS + 5):
    assert status(None) == 401  # no token: refused, but not counted
assert status("1234") == 200
for _ in range(brain.MAX_BAD_TOKENS - 1):
    assert status("0000") == 401
assert status("0000") == 429  # the last allowed wrong token locks it
assert status("1234") == 429  # locked: even the right token is refused
print("test_brain: ok")

if "--live" in sys.argv:
    from claude_agent_sdk import SystemMessage, query

    async def started_with() -> list | None:
        found = None
        async for msg in query(prompt="hi", options=brain.claude_options("Reply with one word.", None)):
            if isinstance(msg, SystemMessage) and msg.subtype == "init":
                found = msg.data["tools"] + msg.data["mcp_servers"]
        return found

    assert asyncio.run(started_with()) == [], "Claude started with tools or connectors"
    print("test_brain --live: ok")
