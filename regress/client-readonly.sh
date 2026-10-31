#!/bin/sh

# Exercise the same CLIENT_READONLY restrictions used by server-access -r
# without needing another UID or privileges.

PATH=/bin:/usr/bin:/usr/local/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest-readonly$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0 1 15

python3 - $TMUX <<'PY' || exit 1
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time

tmux = sys.argv[1:]
failures = []
clients = []

def run(*args):
    return subprocess.check_output(tmux + list(args), text=True).strip()

def check(condition, message):
    if not condition:
        failures.append(message)
        print(message, file=sys.stderr)

class Control:
    def __init__(self, *args, env=None):
        self.process = subprocess.Popen(tmux + ["-C"] + list(args),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, env=env)
        clients.append(self.process)
        self.lines = queue.Queue()
        def read():
            for line in self.process.stdout:
                self.lines.put(line.rstrip("\n"))
            self.lines.put(None)
        threading.Thread(target=read, daemon=True).start()
        error, output = self.reply()
        assert not error, output
        listing = run("list-clients", "-F", "#{client_pid}:#{client_name}")
        self.name = next(line.split(":", 1)[1] for line in listing.splitlines()
            if line.startswith(str(self.process.pid) + ":"))

    def reply(self):
        guard = None
        output = []
        while True:
            line = self.lines.get(timeout=10)
            assert line is not None, "control client exited before its reply"
            if guard is None:
                if line.startswith("%begin "):
                    guard = line[7:]
            elif line in ("%end " + guard, "%error " + guard):
                return line.startswith("%error "), output
            else:
                output.append(line)

    def command(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        return self.reply()

    def allow(self, command):
        error, output = self.command(command)
        check(not error, "command failed: " + command + ": " + str(output))
        return output

    def deny(self, command):
        error, output = self.command(command)
        check(error and "client is read-only" in output,
            "read-only command was not rejected: " + command)

def environment(session):
    return run("show-environment", "-t", session, "DISPLAY")

def state(name):
    return [line for line in run("list-clients", "-F",
        "#{client_name}|#{session_name}|#{client_key_table}|#{client_readonly}")
        .splitlines() if line.startswith(name + "|")]

try:
    with tempfile.TemporaryDirectory(prefix="tmux-readonly-") as directory:
        for session in ("alpha", "beta"):
            run("new-session", "-d", "-s", session, "-c", directory,
                "exec sleep 1000")
            run("set-option", "-t", session, "update-environment", "DISPLAY")
            run("set-environment", "-t", session, "DISPLAY", "owner:0")

        owner = Control("attach", "-E", "-t", "alpha")
        viewer_env = dict(os.environ, DISPLAY="viewer:9")
        viewer = Control("attach", "-r", "-t", "alpha", env=viewer_env)
        check(environment("alpha") == "DISPLAY=owner:0",
            "initial read-only attach updated the session environment")

        # Format jobs must be suppressed even when defaults or a client loop
        # describe a writable client. Ordinary formats must still work.
        marker = os.path.join(directory, "readonly-job")
        viewer.allow("list-clients -F '#(touch " + marker + ")'")
        viewer.allow("list-clients -f '#(touch " + marker + ")'")
        viewer.allow("list-clients -F '#{C:#(touch " + marker + ")}'")
        output = viewer.allow("list-clients -F '#{client_name}:#{session_name}'")
        check(owner.name + ":alpha" in output, "ordinary formats failed")
        time.sleep(0.2)
        check(not os.path.exists(marker), "read-only format started a job")

        # A read-only sender cannot change even a writable target client.
        before = state(owner.name)
        viewer.deny("switch-client -c " + owner.name + " -t beta")
        viewer.deny("switch-client -c " + owner.name + " -T copy-mode")
        viewer.deny("switch-client -c " + owner.name + " -r")
        viewer.deny("send-keys -c " + owner.name + " -t alpha:0 x")
        check(state(owner.name) == before, "another client's state changed")
        if any(line.endswith("|1") for line in state(owner.name)):
            run("switch-client", "-c", owner.name, "-r", "-E")
        run("switch-client", "-c", owner.name, "-t", "alpha", "-E")
        run("switch-client", "-c", owner.name, "-T", "root")

        # Self switching and reattaching remain available, but do not import
        # or remove environment entries from either destination session.
        for session in ("alpha", "beta"):
            run("set-environment", "-t", session, "DISPLAY", "owner:0")
        viewer.allow("switch-client -t beta")
        check(viewer.name + "|beta|root|1" in state(viewer.name),
            "read-only client could not switch its own session")
        check(environment("beta") == "DISPLAY=owner:0",
            "read-only switch updated the session environment")
        viewer.allow("switch-client -T copy-mode")
        check(viewer.name + "|beta|copy-mode|1" in state(viewer.name),
            "read-only client could not change its own key table")
        viewer.allow("attach -t alpha")
        check(environment("alpha") == "DISPLAY=owner:0",
            "read-only reattach updated the session environment")

        empty_env = dict(os.environ)
        empty_env.pop("DISPLAY", None)
        other = Control("attach", "-f", "read-only", "-t", "alpha",
            env=empty_env)
        check(environment("alpha") == "DISPLAY=owner:0",
            "read-only attach removed a session environment entry")

        # Explicit window/pane targets must not change the shared selection.
        run("new-window", "-d", "-t", "alpha:1", "exec sleep 1000")
        run("split-window", "-d", "-t", "alpha:1", "exec sleep 1000")
        selection = run("display-message", "-p", "-t", "alpha",
            "#{window_id}|#{pane_id}")
        window, pane = run("display-message", "-p", "-t", "alpha:1.1",
            "#{window_id}|#{pane_id}").split("|")
        viewer.deny("attach -t alpha:1.1")
        viewer.deny("switch-client -t alpha:1.1")
        viewer.deny("attach -t " + window)
        viewer.deny("attach -t " + pane)
        viewer.deny("switch-client -t " + window)
        viewer.deny("switch-client -t " + pane)
        run("select-pane", "-m", "-t", "alpha:1.1")
        viewer.deny("attach -t '{marked}'")
        viewer.deny("switch-client -t '{marked}'")
        run("select-pane", "-M")
        check(run("display-message", "-p", "-t", "alpha",
            "#{window_id}|#{pane_id}") == selection,
            "read-only client changed the shared window/pane selection")
        viewer.allow("attach -t alpha:0.0")
        viewer.allow("switch-client -t alpha:0.0")

        # Check attach options separately, so a guard for one does not hide
        # missing checks for the others.
        path = run("display-message", "-p", "-t", "alpha", "#{session_path}")
        viewer.deny("attach -c /tmp -t alpha")
        check(run("display-message", "-p", "-t", "alpha", "#{session_path}")
            == path, "read-only attach changed the session path")
        viewer.deny("attach -d -t alpha")
        viewer.deny("attach -x -t alpha")
        check(owner.process.poll() is None, "read-only attach detached owner")
        check(other.process.poll() is None, "read-only attach detached viewer")
        viewer.deny("new-window")
        viewer.deny("run-shell 'touch " + marker + "'")
        viewer.allow("copy-mode -t alpha:0")
        viewer.deny("send-keys -X -c " + owner.name + " -t alpha:0 cursor-down")
        viewer.allow("send-keys -X -t alpha:0 cursor-down")

        # The server owner may still make their own client writable again.
        # Writable commands must continue to import the environment and run
        # jobs as before.
        viewer.allow("switch-client -r -t beta")
        check(environment("beta") == "DISPLAY=viewer:9",
            "writable switch did not update the session environment")
        writable_marker = os.path.join(directory, "writable-job")
        viewer.allow("list-clients -F '#(touch " + writable_marker + ")'")
        for i in range(100):
            if os.path.exists(writable_marker):
                break
            time.sleep(0.05)
        check(os.path.exists(writable_marker), "writable format job did not run")

        # Check permissions when a command executes, including commands that
        # were queued while this client was still writable.
        viewer.process.stdin.write("attach -r -t alpha\nnew-window\n")
        viewer.process.stdin.flush()
        error, output = viewer.reply()
        check(not error, "queued read-only attach failed")
        error, output = viewer.reply()
        check(error and "client is read-only" in output,
            "queued command bypassed the read-only restriction")
finally:
    subprocess.run(tmux + ["kill-server"], stderr=subprocess.DEVNULL)
    for process in clients:
        process.stdin.close()
        process.wait(timeout=10)

sys.exit(bool(failures))
PY
exit 0
