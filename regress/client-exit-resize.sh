#!/bin/sh

# A client that has sent MSG_EXITING can still send MSG_RESIZE. The server
# must not use the tty it closed for MSG_EXITING. Here the client is stopped
# and sent SIGTERM and then SIGWINCH for a size with no pixel size, so the
# server would ask the terminal for one.

PATH=/bin:/usr/bin:/usr/local/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0 1 15

$TMUX new -d 'sleep 1000' || exit 1
$TMUX set-hook -g client-resized 'set -g @resized 1' || exit 1
python3 - $TMUX <<'EOF2' || exit 1
import fcntl, os, pty, signal, struct, subprocess, sys, termios, time

tmux = sys.argv[1:]

def size(fd, rows, cols, xpixel, ypixel):
    fcntl.ioctl(fd, termios.TIOCSWINSZ,
        struct.pack("HHHH", rows, cols, cols * xpixel, rows * ypixel))

master, slave = pty.openpty()
size(slave, 20, 60, 8, 16)
client = os.fork()
if client == 0:
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
    for fd in (0, 1, 2):
        os.dup2(slave, fd)
    os.environ["TERM"] = "xterm"
    os.execvp(tmux[0], tmux + ["attach"])
os.set_blocking(master, False)

# Wait until the server has handled the resize the client sends when it
# attaches.
for i in range(200):
    try:
        os.read(master, 65536)
    except OSError:
        pass
    out = subprocess.run(tmux + ["show", "-gv", "@resized"],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
    if out.strip() == b"1":
        break
    time.sleep(0.05)
else:
    print("the client did not attach")
    sys.exit(1)

# SIGTERM is sent before SIGWINCH, so the client handles it first whether
# it handles pending signals in signal number order or in the order sent.
os.kill(client, signal.SIGSTOP)
os.waitpid(client, os.WUNTRACED)
os.kill(client, signal.SIGTERM)
size(master, 23, 61, 0, 0)
os.kill(client, signal.SIGWINCH)
os.kill(client, signal.SIGCONT)
for i in range(200):
    if os.waitpid(client, os.WNOHANG)[0] != 0:
        break
    time.sleep(0.05)
else:
    print("the client did not exit")
    sys.exit(1)
EOF2
sleep 1
$TMUX has || { echo "the server died"; exit 1; }
exit 0
