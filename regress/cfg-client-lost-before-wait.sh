#!/bin/sh

# A client lost before its configuration callback fires must not start waiting.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 - "$TEST_TMUX" "$(dirname "$0")/../tmux-protocol.h" <<'PY'
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time

tmux = sys.argv[1]
header = Path(sys.argv[2]).read_text()
version = int(re.search(r"#define PROTOCOL_VERSION\s+(\d+)", header)[1])
enum = re.search(r"enum msgtype\s*\{(.*?)\};", header, re.S)[1]
enum = re.sub(r"/\*.*?\*/", "", enum, flags=re.S)
types = {}
value = 0
for entry in enum.split(","):
    entry = entry.strip()
    if not entry:
        continue
    if "=" in entry:
        entry, number = entry.split("=")
        value = int(number.strip())
    types[entry.strip()] = value
    value += 1

def message(name, body=b""):
    return struct.pack("=IIII", types[name], 16 + len(body), version,
        os.getpid()) + body

def wait_for(predicate):
    end = time.monotonic() + 5
    while time.monotonic() < end:
        result = predicate()
        if result:
            return result
        time.sleep(0.05)
    raise AssertionError("timed out waiting for client cleanup")

with tempfile.TemporaryDirectory(prefix="tmux-cfg-early-") as tmp:
    root = Path(tmp)
    sockpath = root / "socket"
    go = root / "go"
    conf = root / "conf"
    conf.write_text("run-shell 'i=0; while [ ! -f %s ] && [ $i -lt 100 ]; "
        "do sleep 0.1; i=$((i + 1)); done'\n" % go)
    server = subprocess.Popen([tmux, "-D", "-v", "-S", str(sockpath),
        "-f", str(conf)], cwd=tmp, stdout=subprocess.DEVNULL)
    logpath = root / ("tmux-server-%d.log" % server.pid)

    def log():
        return logpath.read_text() if logpath.exists() else ""

    try:
        wait_for(sockpath.exists)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.connect(str(sockpath))
            # Both messages are read together. The short MSG_COMMAND makes
            # proc_event_cb lose the client before server_loop runs its queue.
            client.sendall(message("MSG_IDENTIFY_CWD", tmp.encode() + b"\0") +
                message("MSG_IDENTIFY_CLIENTPID", struct.pack("=i", os.getpid())) +
                message("MSG_IDENTIFY_DONE") + message("MSG_COMMAND"))
            lost = wait_for(lambda: re.search(r"lost client (\S+)", log()))[1]
            wait_for(lambda: "free client %s (0 references)" % lost in log())
            contents = log()
            callback = "cmdq_next <client-%d>: [cfg_client_done/" % os.getpid()
            assert contents.index("lost client " + lost) < \
                contents.index(callback), contents
            # Cleanup must happen while configuration is still blocked.
            assert "cmdq_next <global>: [cfg_done/" not in contents, contents
    finally:
        go.touch()
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
PY
