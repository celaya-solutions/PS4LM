#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["mcp>=2,<3"]
# ///
"""MCP server that lets an AI agent drive the PS4 over the cable.

Wraps the same paths the scripts use: elfldr (9021) through run.sh,
GoldHEN FTP (2121), the kernel log (3232) and the Mac log listener (logs.sh).

    uv run --script scripts/ps4_mcp.py        # stdio, started by the MCP client

Settings come from scripts/env.sh (and env.local.sh), like every script.
"""
import ftplib
import io
import os
import socket
import subprocess
import time
from datetime import date
from pathlib import Path

from mcp.server.mcpserver import MCPServer

ROOT = Path(__file__).resolve().parent.parent
JOBS_DIR = ROOT / "logs" / "mcp"


def _env() -> dict:
    out = subprocess.run(["bash", "-c", 'source scripts/env.sh 2>/dev/null; env -0'],
                         cwd=ROOT, capture_output=True, check=True).stdout
    return dict(kv.split("=", 1) for kv in out.decode().split("\0") if "=" in kv)


ENV = _env()
HOST = ENV.get("PS4_HOST", "")
FTP_PORT = int(ENV.get("PS4_FTP_PORT", "2121"))
ELFLDR_PORT = int(ENV.get("PS4_ELFLDR_PORT", "9021"))
LOG_PORT = int(ENV.get("PS4LM_LOG_PORT", "9999"))
KLOG_PORT = 3232

mcp = MCPServer("ps4")
jobs: dict[str, tuple[subprocess.Popen, Path]] = {}


def _open(host: str, port: int) -> bool:
    try:
        socket.create_connection((host, port), timeout=2).close()
        return True
    except OSError:
        return False


def _ftp() -> ftplib.FTP:
    if not HOST:
        raise RuntimeError("PS4_HOST is not set (scripts/env.local.sh)")
    ftp = ftplib.FTP()
    ftp.connect(HOST, FTP_PORT, timeout=10)
    ftp.login()
    return ftp


def _tail(path: Path, max_bytes: int) -> str:
    with open(path, "rb") as f:
        f.seek(max(0, path.stat().st_size - max_bytes))
        return f.read().decode(errors="replace")


@mcp.tool()
def status() -> dict:
    """Which console services answer: elfldr, FTP, klog, the HTTP port (GoldHEN's
    file browser, or llama-server once it runs) and the Mac's log listener."""
    # Never probe the loader port (9090): GoldHEN's BinLoader takes one
    # connection and then stops listening, so a check would burn it.
    return {
        "host": HOST,
        "elfldr_9021": _open(HOST, ELFLDR_PORT),
        "ftp_2121": _open(HOST, FTP_PORT),
        "klog_3232": _open(HOST, KLOG_PORT),
        "http_8080": _open(HOST, 8080),
        "mac_log_listener": _open("127.0.0.1", LOG_PORT),
    }


@mcp.tool()
def setup() -> str:
    """Start elfldr and FTP on a console where GoldHEN is loaded (console-setup.sh).
    Needed once per console boot."""
    p = subprocess.run(["scripts/console-setup.sh"], cwd=ROOT, capture_output=True, timeout=120)
    return (p.stdout + p.stderr).decode(errors="replace") + f"\nexit {p.returncode}"


@mcp.tool()
def list_dir(path: str = "/data/PS4LM") -> str:
    """List a directory on the console over FTP."""
    lines: list[str] = []
    with _ftp() as ftp:
        ftp.retrlines(f"LIST {path}", lines.append)
    return "\n".join(lines)


@mcp.tool()
def read_file(path: str, max_bytes: int = 65536) -> str:
    """Read a text file from the console over FTP. Large files return only
    their last max_bytes (logs grow at the end)."""
    buf = io.BytesIO()
    with _ftp() as ftp:
        ftp.voidcmd("TYPE I")
        size = ftp.size(path) or 0
        start = max(0, size - max_bytes)
        ftp.retrbinary(f"RETR {path}", buf.write, rest=start or None)
    text = buf.getvalue()[:max_bytes].decode(errors="replace")
    return (f"[last {max_bytes} of {size} bytes]\n" if start else "") + text


@mcp.tool()
def upload(local_path: str, remote_path: str) -> str:
    """Copy a file from this Mac to the console over FTP. Creates missing
    directories. Overwrites remote_path if it exists."""
    src = Path(local_path).expanduser()
    with _ftp() as ftp, open(src, "rb") as f:
        parts = remote_path.strip("/").split("/")[:-1]
        for i in range(len(parts)):
            try:
                ftp.mkd("/" + "/".join(parts[: i + 1]))
            except ftplib.error_perm:
                pass  # already there
        ftp.storbinary(f"STOR {remote_path}", f)
    return f"uploaded {src.stat().st_size} bytes to {remote_path}"


def _job_view(job_id: str, max_bytes: int = 16384) -> dict:
    proc, out = jobs[job_id]
    code = proc.poll()
    return {"job": job_id, "running": code is None, "exit": code, "output": _tail(out, max_bytes)}


@mcp.tool()
def run(tool: str, args: list[str] | None = None, wait_seconds: int = 60) -> dict:
    """Upload a tool and run it on the console with arguments (scripts/run.sh).

    tool is a probe name (hello, memprobe), a llama.cpp tool (llama-cli,
    llama-bench, llama-server) or a path to an .elf. Waits up to wait_seconds;
    a tool still running after that keeps going as a job: read it with job().
    Use -t 2 --prio -1 --no-mmap for llama tools (docs/CONSOLE.md)."""
    JOBS_DIR.mkdir(parents=True, exist_ok=True)
    job_id = f"{tool.rsplit('/', 1)[-1]}-{time.strftime('%H%M%S')}"
    out = JOBS_DIR / f"{job_id}.log"
    with open(out, "wb") as f:
        proc = subprocess.Popen(["scripts/run.sh", tool, *(args or [])], cwd=ROOT,
                                stdin=subprocess.DEVNULL, stdout=f, stderr=subprocess.STDOUT)
    jobs[job_id] = (proc, out)
    try:
        proc.wait(timeout=wait_seconds)
    except subprocess.TimeoutExpired:
        pass
    return _job_view(job_id)


@mcp.tool()
def job(job_id: str, stop: bool = False, max_bytes: int = 16384) -> dict:
    """Output and state of a run() job. stop=True closes the Mac side of the
    stream; the console process may keep running until it next writes output."""
    if job_id not in jobs:
        return {"error": f"no job {job_id}", "jobs": list(jobs)}
    if stop:
        jobs[job_id][0].terminate()
        jobs[job_id][0].wait(timeout=5)
    return _job_view(job_id, max_bytes)


@mcp.tool()
def klog(seconds: float = 5, max_bytes: int = 32768) -> str:
    """Read the console's kernel log (port 3232) for some seconds. Shows
    crashes, 'No heart beat of Shell UI' and 'VM Stats ... swap out'."""
    buf = bytearray()
    end = time.monotonic() + seconds
    with socket.create_connection((HOST, KLOG_PORT), timeout=5) as s:
        while (left := end - time.monotonic()) > 0:
            s.settimeout(left)
            try:
                chunk = s.recv(65536)
            except socket.timeout:
                break
            if not chunk:
                break
            buf += chunk
    return bytes(buf[-max_bytes:]).decode(errors="replace")


@mcp.tool()
def mac_logs(max_bytes: int = 16384) -> str:
    """Tail of today's log from the Mac listener (scripts/logs.sh), where the
    console tools send their log lines."""
    log = ROOT / "logs" / f"{date.today():%Y-%m-%d}.log"
    if not log.exists():
        return f"no {log.name}; is scripts/logs.sh running?"
    return _tail(log, max_bytes)


if __name__ == "__main__":
    os.chdir(ROOT)
    mcp.run()
