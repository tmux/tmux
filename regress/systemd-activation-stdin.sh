#!/bin/sh

# A socket activated server must not close its standard input.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

# Activation needs systemd support, and /proc shows the server's descriptors.
ldd "$TEST_TMUX" 2>/dev/null | grep -q libsystemd || exit 0
[ -d /proc/self/fd ] || exit 0

python3 - "$TEST_TMUX" <<'PY'
import os
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

    # Start the server the way systemd does: the listening socket on
    # descriptor 3, LISTEN_PID naming the server and stdin on /dev/null.
    pid = os.fork()
    if pid == 0:
        os.dup2(os.open(os.devnull, os.O_RDONLY), 0)
        os.dup2(listener.fileno(), 3)
        os.set_inheritable(3, True)
        os.closerange(4, 1024)
        os.execve(tmux, [tmux, "-D", "-S", path, "-f/dev/null"],
            dict(env, LISTEN_FDS="1", LISTEN_PID=str(os.getpid())))
    listener.close()

    try:
        subprocess.run([tmux, "-S", path, "new", "-d"], env=env,
            check=True, timeout=5)
        fd0 = os.readlink("/proc/%d/fd/0" % pid)
        assert fd0 == os.devnull, fd0
    finally:
        subprocess.run([tmux, "-S", path, "kill-server"], env=env,
            timeout=5)
        os.waitpid(pid, 0)
PY
