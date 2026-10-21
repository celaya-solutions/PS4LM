#!/usr/bin/env python3
"""Long-chat test for llama-server on the console.

Plays one growing conversation against the server's OpenAI chat endpoint
until the next turn would not fit in the context, then asks for a word given
in the first turn, so the run shows speed per turn as the history grows and
whether the model still uses the oldest part of it. Last, it sends one request
that is too long on purpose and checks the server is still healthy after it.

    scripts/longchat.py                       # http://$PS4_HOST:8080
    scripts/longchat.py --url http://192.168.50.2:8080 --max-tokens 160

Stdlib only. Thinking is switched off (Qwen3.5 otherwise spends max_tokens on
it). Prints one line per turn and a JSON summary at the end.
"""
import argparse
import json
import os
import time
import urllib.error
import urllib.request

SECRET = "periwinkle"

TOPICS = [
    "What makes a good co-op game? Give three points.",
    "Describe the PS4 controller to someone who has never held one.",
    "Write a short pep talk for a player stuck on a hard boss.",
    "Explain what a save file is, in plain words.",
    "Name three ways a game can teach its controls without a manual.",
    "What is the difference between frame rate and resolution?",
    "Give a two-sentence story about a robot that loves racing games.",
    "Why do some games have a day and night cycle?",
    "List four tips for a first playthrough of a souls-like game.",
    "Explain what a loading screen is doing behind the scenes.",
    "What makes a boss fight memorable? Three points.",
    "Describe a cozy game in three sentences.",
    "How would you explain a skill tree to a child?",
    "Why do games have difficulty settings?",
    "Write a haiku about a console fan spinning up.",
    "What is a speedrun, and why do people do them?",
]


def post(url, body, timeout):
    req = urllib.request.Request(
        url, data=json.dumps(body).encode(), headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def get(url, timeout=10):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.load(r)


def chat(base, messages, max_tokens, timeout):
    t0 = time.time()
    out = post(
        base + "/v1/chat/completions",
        {
            "messages": messages,
            "max_tokens": max_tokens,
            "temperature": 0.7,
            "chat_template_kwargs": {"enable_thinking": False},
        },
        timeout,
    )
    out["_wall"] = time.time() - t0
    return out


def main():
    ap = argparse.ArgumentParser()
    host = os.environ.get("PS4_HOST", "192.168.50.2")
    ap.add_argument("--url", default=f"http://{host}:8080")
    ap.add_argument("--max-tokens", type=int, default=160)
    ap.add_argument("--timeout", type=float, default=900)
    a = ap.parse_args()

    props = get(a.url + "/props")
    n_ctx = props["default_generation_settings"]["n_ctx"]
    print(f"server n_ctx {n_ctx}, build {props.get('build_info', '?')}")

    messages = [
        {"role": "system", "content": "You are a friendly gaming buddy. Keep answers short."},
        {"role": "user", "content": f"Remember this word for later: {SECRET}. Just say OK."},
    ]
    turns = []
    i = 0
    while True:
        out = chat(a.url, messages, a.max_tokens, a.timeout)
        u, t = out["usage"], out.get("timings", {})
        reply = out["choices"][0]["message"]["content"]
        messages.append({"role": "assistant", "content": reply})
        row = {
            "turn": len(turns) + 1,
            "history": u["prompt_tokens"],
            "new_prompt": t.get("prompt_n"),
            "prompt_tps": round(t.get("prompt_per_second", 0), 2),
            "gen": u["completion_tokens"],
            "gen_tps": round(t.get("predicted_per_second", 0), 2),
            "wall_s": round(out["_wall"], 1),
        }
        turns.append(row)
        print(json.dumps(row))
        # Room left for one more question, its answer and the final recall turn.
        if u["total_tokens"] + 2 * a.max_tokens + 80 > n_ctx:
            break
        messages.append({"role": "user", "content": TOPICS[i % len(TOPICS)]})
        i += 1

    messages.append(
        {"role": "user", "content": "What was the word I asked you to remember at the start? One word."}
    )
    out = chat(a.url, messages, 16, a.timeout)
    recall = out["choices"][0]["message"]["content"].strip()
    recall_row = {
        "history": out["usage"]["prompt_tokens"],
        "answer": recall,
        "correct": SECRET in recall.lower(),
        "wall_s": round(out["_wall"], 1),
    }
    print("recall", json.dumps(recall_row))

    # One request bigger than the context: the server should refuse it and live.
    overflow = {"status": None}
    try:
        chat(a.url, [{"role": "user", "content": "word " * (n_ctx + 200)}], 8, a.timeout)
        overflow["status"] = 200
    except urllib.error.HTTPError as e:
        overflow["status"] = e.code
        overflow["error"] = e.read().decode(errors="replace")[:200]
    overflow["health_after"] = get(a.url + "/health").get("status")
    print("overflow", json.dumps(overflow))

    print(
        "summary",
        json.dumps({"n_ctx": n_ctx, "turns": turns, "recall": recall_row, "overflow": overflow}),
    )


if __name__ == "__main__":
    main()
