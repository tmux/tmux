#!/bin/sh

# SIGUSR1 must not replace the socket of a socket activated server.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

# Activation needs systemd support.
ldd "$TEST_TMUX" 2>/dev/null | grep -q libsystemd || exit 0

python3 - "$TEST_TMUX" <<'PY'
import os
import signal
import socket
import subprocess
import sys
import tempfile

tmux = sys.argv[1]
env = { "PATH": "/bin:/usr/bin", "TERM": "screen" }

with tempfile.TemporaryDirectory() as tmp:
    path = os.path.join(tmp, "socket")
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind(path)
    listener.listen()
    before = os.stat(path)

    # Start the server the way systemd does: the listening socket on
    # descriptor 3 and LISTEN_PID naming the server. This process keeps its
    # copy of the socket open, as the service manager does.
    pid = os.fork()
    if pid == 0:
        os.dup2(os.open(os.devnull, os.O_RDONLY), 0)
        os.dup2(listener.fileno(), 3)
        os.set_inheritable(3, True)
        os.closerange(4, 1024)
        os.execve(tmux, [tmux, "-D", "-S", path, "-f/dev/null"],
            dict(env, LISTEN_FDS="1", LISTEN_PID=str(os.getpid())))

    try:
        subprocess.run([tmux, "-S", path, "new", "-d"], env=env,
            check=True, timeout=5)
        os.kill(pid, signal.SIGUSR1)
        subprocess.run([tmux, "-S", path, "has"], env=env,
            check=True, timeout=5)
        after = os.stat(path)
        assert (after.st_dev, after.st_ino) == \
            (before.st_dev, before.st_ino), "socket replaced"
    finally:
        subprocess.run([tmux, "-S", path, "kill-server"], env=env,
            timeout=5)
        os.waitpid(pid, 0)

    # With the server gone, a client must still reach the service manager.
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(path)
    client.close()
    listener.close()
PY
