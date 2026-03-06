#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
export TEST_TMUX

python3 - <<'PY'
import os
import pty
import re
import select
import shlex
import signal
import subprocess
import sys
import time

TMUX = os.environ["TEST_TMUX"]
SOCK = "testkitty"


def run(*args, check=True):
    return subprocess.run(
        [TMUX, f"-L{SOCK}", *args],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def tmux_bool(target, fmt):
    out = run("display-message", "-p", "-t", target, fmt, check=False)
    if out.returncode != 0:
        return None
    return out.stdout.strip()


def capture(target):
    out = run("capture-pane", "-p", "-J", "-S", "-", "-t", target, check=False)
    if out.returncode != 0:
        return ""
    return out.stdout


def new_window(command):
    out = run("new-window", "-d", "-P", "-F", "#{window_id}", command)
    return out.stdout.strip()


def kill_window(target):
    run("kill-window", "-t", target, check=False)


def wait_for_ready(target, marker, timeout=3):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if marker in capture(target):
            return True
        time.sleep(0.05)
    return False


def wait_for_dead(target, timeout=3):
    deadline = time.time() + timeout
    while time.time() < deadline:
        dead = tmux_bool(target, "#{pane_dead}")
        if dead == "1":
            return True
        if dead is None:
            return False
        time.sleep(0.05)
    return False


def python_command(code):
    return "python3 -c " + shlex.quote(code)


def drain_client(fd, timeout=0.05, rounds=20):
    data = b""
    for _ in range(rounds):
        r, _, _ = select.select([fd], [], [], timeout)
        if not r:
            break
        try:
            chunk = os.read(fd, 4096)
        except OSError:
            break
        if not chunk:
            break
        data += chunk
    return data


def parse_hex_line(text):
    matches = re.findall(r"HEX:([0-9a-f]*)", text)
    if not matches:
        return None
    return bytes.fromhex(matches[-1])


def expect_bytes(label, mode_seq, tmux_key, checker):
    code = (
        "import os, select, sys, termios, time, tty\n"
        f"mode = bytes.fromhex('{mode_seq.hex()}')\n"
        "fd = sys.stdin.fileno()\n"
        "old = termios.tcgetattr(fd)\n"
        "try:\n"
        "    tty.setraw(fd)\n"
        "    if mode:\n"
        "        os.write(sys.stdout.fileno(), mode)\n"
        "    os.write(sys.stdout.fileno(), b'READY\\n')\n"
        "    data = b''\n"
        "    seen = False\n"
        "    deadline = time.time() + 2\n"
        "    while time.time() < deadline:\n"
        "        r, _, _ = select.select([fd], [], [], 0.1)\n"
        "        if not r:\n"
        "            if seen:\n"
        "                break\n"
        "            continue\n"
        "        chunk = os.read(fd, 1024)\n"
        "        if not chunk:\n"
        "            break\n"
        "        data += chunk\n"
        "        seen = True\n"
        "    os.write(sys.stdout.fileno(), b'HEX:' + data.hex().encode() + b'\\n')\n"
        "finally:\n"
        "    termios.tcsetattr(fd, termios.TCSANOW, old)\n"
    )

    target = new_window(python_command(code))
    ok = False
    try:
        if not wait_for_ready(target, "READY"):
            print(f"[FAIL] {label} -> probe not ready")
            return False

        run("send-keys", "-t", target, tmux_key)
        if not wait_for_dead(target):
            print(f"[FAIL] {label} -> probe did not exit")
            return False

        data = parse_hex_line(capture(target))
        if data is None:
            print(f"[FAIL] {label} -> missing HEX line")
            return False

        ok = checker(data)
        if ok:
            print(f"[PASS] {label} -> {data.hex()}")
        else:
            print(f"[FAIL] {label} -> unexpected bytes {data.hex()}")
        return ok
    finally:
        kill_window(target)


def expect_query_states(expected):
    tests = [
        ("SET2", "\x1b[=2u"),
        ("SET9", "\x1b[=9u"),
        ("PUSH16", "\x1b[>16u"),
        ("POP1", "\x1b[<u"),
        ("SET16", "\x1b[=16u"),
        ("PUSH9", "\x1b[>9u"),
        ("PUSH1", "\x1b[>1u"),
        ("POP2", "\x1b[<2u"),
    ]

    code = (
        "import os, re, select, sys, termios, time, tty\n"
        "fd = sys.stdin.fileno()\n"
        "old = termios.tcgetattr(fd)\n"
        "tests = [\n"
        + "\n".join(
            f"    ({name!r}, {seq.encode('latin1')!r}),"
            for name, seq in tests
        )
        + "\n]\n"
        "pattern = re.compile(rb'\\x1b\\[\\?(\\d+)u')\n"
        "def read_reply(timeout=1.5):\n"
        "    data = b''\n"
        "    deadline = time.time() + timeout\n"
        "    while time.time() < deadline:\n"
        "        r, _, _ = select.select([fd], [], [], 0.1)\n"
        "        if not r:\n"
        "            continue\n"
        "        chunk = os.read(fd, 1024)\n"
        "        if not chunk:\n"
        "            break\n"
        "        data += chunk\n"
        "        m = pattern.search(data)\n"
        "        if m is not None:\n"
        "            return int(m.group(1))\n"
        "    return None\n"
        "try:\n"
        "    tty.setraw(fd)\n"
        "    os.write(sys.stdout.fileno(), b'\\x1b[?u')\n"
        "    _ = read_reply()\n"
        "    for name, seq in tests:\n"
        "        os.write(sys.stdout.fileno(), seq)\n"
        "        os.write(sys.stdout.fileno(), b'\\x1b[?u')\n"
        "        value = read_reply()\n"
        "        if value is None:\n"
        "            os.write(sys.stdout.fileno(), b'\\x1b[?u')\n"
        "            value = read_reply()\n"
        "        if value is None:\n"
        "            os.write(sys.stdout.fileno(), f'{name}:NONE\\r\\n'.encode())\n"
        "        else:\n"
        "            os.write(sys.stdout.fileno(), f'{name}:{value}\\r\\n'.encode())\n"
        "finally:\n"
        "    termios.tcsetattr(fd, termios.TCSANOW, old)\n"
    )

    target = new_window(python_command(code))
    try:
        if not wait_for_dead(target):
            print("[FAIL] kitty mode query probe did not exit")
            return False
        text = capture(target)

        seen = {}
        for line in text.splitlines():
            m = re.match(r"^(SET2|SET9|PUSH16|POP1|SET16|PUSH9|PUSH1|POP2):(\d+|NONE)$", line.strip())
            if m is None:
                continue
            seen[m.group(1)] = None if m.group(2) == "NONE" else int(m.group(2))

        ok = True
        for name, value in expected.items():
            actual = seen.get(name)
            if actual != value:
                print(f"[FAIL] query {name} -> expected {value}, got {actual}")
                ok = False
            else:
                print(f"[PASS] query {name} -> {actual}")
        return ok
    finally:
        kill_window(target)


def expect_key_name(client_fd, seq, expected):
    proc = subprocess.Popen(
        [TMUX, f"-L{SOCK}", "command-prompt", "-k", "display-message -pl '%%'"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    time.sleep(0.05)
    os.write(client_fd, seq)

    try:
        out, _ = proc.communicate(timeout=2)
        actual = out.strip()
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
        actual = "<timeout>"

    if actual == expected:
        print(f"[PASS] {seq!r} -> {actual}")
        return True

    print(f"[FAIL] {seq!r} -> expected {expected}, got {actual}")
    return False

def expect_no_key(client_fd, seq, label, timeout=0.5):
    proc = subprocess.Popen(
        [TMUX, f"-L{SOCK}", "command-prompt", "-k", "display-message -pl '%%'"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    time.sleep(0.05)
    os.write(client_fd, seq)

    try:
        out, _ = proc.communicate(timeout=timeout)
        actual = out.strip()
        if actual == "":
            print(f"[PASS] {label} -> no key parsed")
            return True
        print(f"[FAIL] {label} -> expected no key, got {actual}")
        return False
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
        print(f"[PASS] {label} -> no key parsed")
        return True



def expect_push_pop_balance(client_fd, client_pid):
    output = drain_client(client_fd)
    for _ in range(3):
        os.write(client_fd, b"\x1b[?0u")
        time.sleep(0.1)
        output += drain_client(client_fd)

    try:
        os.kill(client_pid, signal.SIGHUP)
    except ProcessLookupError:
        pass
    time.sleep(0.3)
    output += drain_client(client_fd, rounds=40)

    pushes = output.count(b"\x1b[>1u")
    pops = output.count(b"\x1b[<1u")

    ok = True
    if pushes != 1:
        print(f"[FAIL] push count -> expected 1, got {pushes}")
        ok = False
    else:
        print(f"[PASS] push count -> {pushes}")

    if pops < 1:
        print(f"[FAIL] pop count -> expected >=1, got {pops}")
        ok = False
    else:
        print(f"[PASS] pop count -> {pops}")

    return ok


failed = False
client_pid = None
client_fd = None

try:
    run("kill-server", check=False)
    run("-f/dev/null", "new", "-d")
    run("set", "-g", "kitty-keys", "always")
    run("set", "-g", "remain-on-exit", "on")
    run("set", "-g", "status", "off")

    client_pid, client_fd = pty.fork()
    if client_pid == 0:
        os.execvp(TMUX, [TMUX, f"-L{SOCK}", "attach"])

    time.sleep(0.5)
    drain_client(client_fd)

    # Phase 1/3 parser coverage: functional tilde aliases.
    for seq, expected in [
        (b"\x1b[7~", "Home"),
        (b"\x1b[8~", "End"),
        (b"\x1b[11~", "F1"),
        (b"\x1b[12~", "F2"),
        (b"\x1b[14~", "F4"),
    ]:
        if not expect_key_name(client_fd, seq, expected):
            failed = True

    # Phase 2 parser determinism: unsupported progressive fields are discarded.
    for label, seq in [
        ("unsupported event field", b"\x1b[99;5:2u"),
        ("unsupported alternates field", b"\x1b[99:67:99;5u"),
        ("unsupported text field", b"\x1b[99;5;99u"),
    ]:
        if not expect_no_key(client_fd, seq, label):
            failed = True


    # Phase 1 output semantics.
    if not expect_bytes(
        "disambiguate C-c",
        b"\x1b[=1u",
        "C-c",
        lambda b: b == b"\x1b[99;5u",
    ):
        failed = True
    if not expect_bytes(
        "disambiguate C-i",
        b"\x1b[=1u",
        "C-i",
        lambda b: b == b"\x1b[105;5u",
    ):
        failed = True
    if not expect_bytes(
        "disambiguate C-m",
        b"\x1b[=1u",
        "C-m",
        lambda b: b == b"\x1b[109;5u",
    ):
        failed = True
    if not expect_bytes(
        "disambiguate M-c",
        b"\x1b[=1u",
        "M-c",
        lambda b: b == b"\x1b[99;3u",
    ):
        failed = True
    if not expect_bytes(
        "disambiguate C-M-c",
        b"\x1b[=1u",
        "C-M-c",
        lambda b: b == b"\x1b[99;7u",
    ):
        failed = True

    # Enter/Tab/Backspace legacy exceptions when report-all is disabled.
    if not expect_bytes(
        "disambiguate Tab legacy",
        b"\x1b[=1u",
        "Tab",
        lambda b: b == b"\t",
    ):
        failed = True
    if not expect_bytes(
        "disambiguate Enter legacy",
        b"\x1b[=1u",
        "Enter",
        lambda b: b == b"\r",
    ):
        failed = True
    if not expect_bytes(
        "disambiguate Backspace legacy",
        b"\x1b[=1u",
        "BSpace",
        lambda b: b in (b"\x7f", b"\x08"),
    ):
        failed = True

    # Enter/Tab/Backspace CSI-u in report-all mode.
    if not expect_bytes(
        "report-all Tab CSI-u",
        b"\x1b[=8u",
        "Tab",
        lambda b: b == b"\x1b[9u",
    ):
        failed = True
    if not expect_bytes(
        "report-all Enter CSI-u",
        b"\x1b[=8u",
        "Enter",
        lambda b: b == b"\x1b[13u",
    ):
        failed = True
    if not expect_bytes(
        "report-all Backspace CSI-u",
        b"\x1b[=8u",
        "BSpace",
        lambda b: b in (b"\x1b[127u", b"\x1b[8u"),
    ):
        failed = True

    # Phase 2: unsupported flags are explicitly gated in set/push/query semantics.
    if not expect_query_states(
        {
            "SET2": 0,
            "SET9": 9,
            "PUSH16": 0,
            "POP1": 9,
            "SET16": 0,
            "PUSH9": 9,
            "PUSH1": 1,
            "POP2": 0,
        }
    ):
        failed = True

    # Phase 4: repeated query responses must not cause duplicate pushes.
    if not expect_push_pop_balance(client_fd, client_pid):
        failed = True

    if failed:
        sys.exit(1)
finally:
    if client_pid is not None:
        try:
            os.kill(client_pid, signal.SIGHUP)
        except ProcessLookupError:
            pass
    subprocess.run(
        [TMUX, f"-L{SOCK}", "kill-server"],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
PY
