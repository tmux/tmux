#!/bin/sh

# A layout resize must notify synchronized applications before the pending
# geometry redraw is presented. Their first post-resize frames are collected in
# the backing screens and the client receives one final redraw, not a stale
# geometry frame followed by one correction per pane.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 - "$TEST_TMUX" <<'PY'
import fcntl
import os
import re
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time


tmux = sys.argv[1]
label = "test-resize-sync-redraw-%d" % os.getpid()
server = [tmux, "-L" + label, "-f/dev/null"]
width = 100
height = 30
sync_start = b"\033[?2026h"
sync_end = b"\033[?2026l"
barrier_deadline = 0.15
prompt_release_deadline = 0.13
release_quiet = barrier_deadline + 0.15

emitter_source = r'''
import os
import signal
import sys
import time


name, update_mode, ready, arm, resized, winches, gate, painted, preopen, \
    preopened, preclose, preclosed, partial, partial_started, \
    partial_finish, skip, repeat, repeat_started, repeat_stop, exit_frame, \
    exit_started, flood, flood_started, flood_stop, flood_done, same_batch, \
    same_batch_written = sys.argv[1:]
pending = False
generation = 0
resize_times = []


def touch(path):
    with open(path, "w", encoding="utf-8"):
        pass


def on_resize(_signum, _frame):
    global pending
    size = os.get_terminal_size(1)
    resize_times.append((time.monotonic(), size.columns, size.lines))
    if os.path.exists(arm):
        pending = True


def paint(phase):
    size = os.get_terminal_size(1)
    rows = []
    for row in range(size.lines):
        marker = "%s_%s_%02d_" % (name, phase, row)
        fill = chr(ord("A") + (row * 7 + len(phase)) % 26)
        rows.append((marker + fill * size.columns)[:size.columns])
    start = "\033[?2026h" if update_mode != "plain" else ""
    end = "\033[?2026l" if update_mode != "plain" else ""
    payload = (start + "\033[H" + "\r\n".join(rows) + end).encode()
    written = os.write(1, payload)
    if written != len(payload):
        raise RuntimeError("short frame write")


def paint_partial(phase):
    size = os.get_terminal_size(1)
    split = max(1, size.lines // 2)
    top = []
    bottom = []
    for row in range(size.lines):
        half = "TOP" if row < split else "BOTTOM"
        marker = "%s_%s_%s_%02d_" % (name, phase, half, row)
        fill = chr(ord("A") + (row * 7 + len(phase)) % 26)
        line = (marker + fill * size.columns)[:size.columns]
        if row < split:
            top.append(line)
        else:
            bottom.append(line)
    first = ("\033[?2026h\033[H" + "\r\n".join(top)).encode()
    written = os.write(1, first)
    if written != len(first):
        raise RuntimeError("short partial frame start write")
    touch(partial_started)
    while not os.path.exists(partial_finish):
        time.sleep(0.001)
    os.unlink(partial_finish)
    second = ("\r\n" + "\r\n".join(bottom) + "\033[?2026l").encode()
    written = os.write(1, second)
    if written != len(second):
        raise RuntimeError("short partial frame finish write")


def paint_repeated(phase):
    size = os.get_terminal_size(1)
    marker = ("%s_%s_REPEAT_" % (name, phase)).ljust(size.columns, "R")
    payload = ("\033[?2026h\033[H" + marker).encode()
    written = os.write(1, payload)
    if written != len(payload):
        raise RuntimeError("short repeated frame start write")
    touch(repeat_started)
    next_repeat = time.monotonic() + 0.1
    while not os.path.exists(repeat_stop):
        if time.monotonic() >= next_repeat:
            os.write(1, b"\033[?2026h")
            next_repeat = time.monotonic() + 0.1
        time.sleep(0.001)
    os.write(1, b"\033[?2026l")


def paint_and_exit(phase):
    size = os.get_terminal_size(1)
    marker = ("%s_%s_EXIT_" % (name, phase)).ljust(size.columns, "X")
    payload = ("\033[?2026h\033[H" + marker).encode()
    written = os.write(1, payload)
    if written != len(payload):
        raise RuntimeError("short exiting frame write")
    touch(exit_started)
    os._exit(0)


def write_backpressure():
    payload = ((name + "_BACKPRESSURE_\r\n") * 256).encode()[:4096]
    touch(flood_started)
    while not os.path.exists(flood_stop):
        os.write(1, payload)
    touch(flood_done)


def write_same_batch():
    # Must stay far below the smallest pty buffer: the server is stopped
    # while this runs, so a write larger than the buffer blocks and the
    # marker below is never written. macOS buffers much less than Linux.
    payload = ("\033[2J\033[H" + name + "_SAME_BATCH_").encode()
    written = os.write(1, payload)
    if written != len(payload):
        raise RuntimeError("short same-batch write")
    touch(same_batch_written)


def paint_open():
    size = os.get_terminal_size(1)
    marker = ("%s_OPEN_" % name).ljust(size.columns, "O")
    payload = ("\033[?2026h\033[H" + marker).encode()
    written = os.write(1, payload)
    if written != len(payload):
        raise RuntimeError("short open frame write")
    touch(preopened)


def close_preopen():
    if not os.path.exists(preclose):
        return
    os.unlink(preclose)
    written = os.write(1, b"\033[?2026l")
    if written != len(b"\033[?2026l"):
        raise RuntimeError("short pre-resize frame close write")
    touch(preclosed)


signal.signal(signal.SIGWINCH, on_resize)
if update_mode != "sync-primary":
    os.write(1, b"\033[?1049h")
paint("BEFORE")
touch(ready)

while True:
    if resize_times:
        with open(winches, "a", encoding="utf-8") as handle:
            for resize_time, columns, lines in resize_times:
                handle.write("%.9f %d %d\n" %
                    (resize_time, columns, lines))
        resize_times.clear()
    close_preopen()
    if os.path.exists(same_batch):
        os.unlink(same_batch)
        write_same_batch()
        continue
    if os.path.exists(flood):
        os.unlink(flood)
        write_backpressure()
        continue
    if os.path.exists(preopen):
        os.unlink(preopen)
        paint_open()
        continue
    if not pending:
        time.sleep(0.001)
        continue
    pending = False
    generation += 1
    touch(resized)
    while not os.path.exists(gate):
        close_preopen()
        time.sleep(0.001)
    if os.path.exists(skip):
        pass
    elif os.path.exists(repeat):
        paint_repeated("AFTER%d" % generation)
    elif os.path.exists(exit_frame):
        paint_and_exit("AFTER%d" % generation)
    elif os.path.exists(partial):
        paint_partial("AFTER%d" % generation)
    else:
        paint("AFTER%d" % generation)
    touch(painted)
'''


def run(*args, check=True):
    return subprocess.run(
        server + list(args),
        check=check,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def fail(message):
    raise RuntimeError(message)


def wait_for(predicate, timeout, message):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.005)
    fail(message)


def read_available(fd, duration):
    output = bytearray()
    deadline = time.monotonic() + duration
    while time.monotonic() < deadline:
        readable, _, _ = select.select([fd], [], [], 0.005)
        if fd not in readable:
            continue
        try:
            output.extend(os.read(fd, 1 << 20))
        except (BlockingIOError, OSError):
            break
    return bytes(output)


def read_before(fd, deadline, message):
    while time.monotonic() < deadline:
        timeout = min(0.005, deadline - time.monotonic())
        readable, _, _ = select.select([fd], [], [], timeout)
        if fd not in readable:
            continue
        try:
            output = os.read(fd, 1 << 20)
        except (BlockingIOError, OSError):
            output = b""
        if output:
            return output
    fail(message)


def drain_until_quiet(fd, quiet, timeout):
    output = bytearray()
    deadline = time.monotonic() + timeout
    quiet_deadline = time.monotonic() + quiet
    while time.monotonic() < deadline:
        readable, _, _ = select.select([fd], [], [], 0.005)
        if fd in readable:
            try:
                chunk = os.read(fd, 1 << 20)
            except (BlockingIOError, OSError):
                chunk = b""
            if chunk:
                output.extend(chunk)
                quiet_deadline = time.monotonic() + quiet
                continue
        if time.monotonic() >= quiet_deadline:
            return bytes(output)
    fail("client output did not become quiet")


def attach(session="resize", columns=width, lines=height):
    pid, fd = os.forkpty()
    if pid == 0:
        fcntl.ioctl(
            1,
            termios.TIOCSWINSZ,
            struct.pack("HHHH", lines, columns, 0, 0),
        )
        os.environ["TERM"] = "xterm-256color"
        os.execl(
            tmux,
            tmux,
            "-L" + label,
            "-f/dev/null",
            "attach-session",
            "-t",
            session,
        )
    os.set_blocking(fd, False)
    return pid, fd


def complete_client_startup(fd, expected_clients=1):
    query = b"\033[>q"
    output = bytearray()
    deadline = time.monotonic() + 5
    while query not in output:
        output.extend(read_before(
            fd,
            deadline,
            "client did not request extended device attributes",
        ))

    terminal_type = b"XTerm(370)"
    response = b"\033P>|" + terminal_type + b"\033\\"
    if os.write(fd, response) != len(response):
        fail("short extended device attributes response")
    wait_for(
        lambda: run(
            "list-clients", "-F", "#{client_termtype}"
        ).stdout.splitlines().count(terminal_type) >= expected_clients,
        2,
        "server did not process extended device attributes response",
    )
    drain_until_quiet(fd, 0.2, 5)


def close_client(pid=None, fd=None):
    if fd is not None:
        try:
            os.close(fd)
        except OSError:
            pass
    if pid is not None:
        try:
            os.kill(pid, signal.SIGHUP)
        except ProcessLookupError:
            pass


def cleanup(pid=None, fd=None):
    close_client(pid, fd)
    run("kill-server", check=False)


run("kill-server", check=False)
with tempfile.TemporaryDirectory() as directory:
    emitter = os.path.join(directory, "emitter.py")
    with open(emitter, "w", encoding="utf-8") as handle:
        handle.write(emitter_source)

    paths = {}
    for name in ("LEFT", "RIGHT"):
        paths[name] = {
            key: os.path.join(directory, "%s-%s" % (name.lower(), key))
            for key in ("ready", "arm", "resized", "winches", "gate", "painted",
                "preopen", "preopened", "preclose", "preclosed", "partial",
                "partial_started", "partial_finish", "skip", "repeat",
                "repeat_started", "repeat_stop", "exit_frame", "exit_started",
                "flood", "flood_started", "flood_stop", "flood_done",
                "same_batch", "same_batch_written")
        }

    def command(name, synchronized=True, alternate=True):
        pane = paths[name]
        values = [
            name,
            ("sync" if alternate else "sync-primary")
                if synchronized else "plain",
            pane["ready"],
            pane["arm"],
            pane["resized"],
            pane["winches"],
            pane["gate"],
            pane["painted"],
            pane["preopen"],
            pane["preopened"],
            pane["preclose"],
            pane["preclosed"],
            pane["partial"],
            pane["partial_started"],
            pane["partial_finish"],
            pane["skip"],
            pane["repeat"],
            pane["repeat_started"],
            pane["repeat_stop"],
            pane["exit_frame"],
            pane["exit_started"],
            pane["flood"],
            pane["flood_started"],
            pane["flood_stop"],
            pane["flood_done"],
            pane["same_batch"],
            pane["same_batch_written"],
        ]
        return "python3 -u %s %s" % (emitter, " ".join(values))

    def clear_controls():
        for pane in paths.values():
            for path in pane.values():
                if os.path.exists(path):
                    os.unlink(path)

    def respawn_emitters(right_synchronized=True, left_alternate=True):
        clear_controls()
        for pane_id in (left, right):
            run("set-option", "-p", "-t", pane_id,
                "remain-on-exit", "off")
        run("respawn-pane", "-k", "-t", left,
            command("LEFT", alternate=left_alternate))
        run("respawn-pane", "-k", "-t", right,
            command("RIGHT", right_synchronized))
        wait_for(
            lambda: all(os.path.exists(paths[name]["ready"])
                for name in paths),
            5,
            "respawned applications did not become ready",
        )
        wait_for(
            lambda: b"LEFT_BEFORE_00_" in run(
                "capture-pane", "-p", "-t", left
            ).stdout and b"RIGHT_BEFORE_00_" in run(
                "capture-pane", "-p", "-t", right
            ).stdout,
            5,
            "respawned initial frames did not reach both backing screens",
        )
        initial = bytearray()
        initial_deadline = time.monotonic() + 5
        while (b"LEFT_BEFORE_00_" not in initial or
                b"RIGHT_BEFORE_00_" not in initial):
            initial.extend(read_before(
                fd,
                initial_deadline,
                "respawned initial frames did not reach the client",
            ))
        drain_until_quiet(fd, 0.2, 5)

    pid = None
    fd = None
    watch_pid = None
    watch_fd = None
    try:
        run(
            "new-session",
            "-d",
            "-s",
            "resize",
            "-x",
            str(width),
            "-y",
            str(height),
            command("LEFT"),
        )
        left = run(
            "display-message", "-p", "#{pane_id}"
        ).stdout.decode().strip()
        right = run(
            "split-window",
            "-d",
            "-h",
            "-t",
            left,
            "-PF",
            "#{pane_id}",
            command("RIGHT"),
        ).stdout.decode().strip()
        run("set-option", "-g", "status", "off")
        run("set-option", "-g", "set-titles", "off")
        run("set-option", "-g", "window-size", "manual")
        run("set-option", "-g", "automatic-rename", "off")
        run("set-option", "-as", "terminal-features", "*:sync")
        wait_for(
            lambda: all(os.path.exists(paths[name]["ready"])
                for name in paths),
            5,
            "synchronized applications did not become ready",
        )
        wait_for(
            lambda: b"LEFT_BEFORE_00_" in run(
                "capture-pane", "-p", "-t", left
            ).stdout and b"RIGHT_BEFORE_00_" in run(
                "capture-pane", "-p", "-t", right
            ).stdout,
            5,
            "initial synchronized frames did not reach both backing screens",
        )

        pid, fd = attach()
        complete_client_startup(fd)
        wait_for(
            lambda: b"sync" in run(
                "list-clients", "-F", "#{client_termfeatures}"
            ).stdout,
            5,
            "sync-capable client did not attach",
        )
        client_size = run(
            "list-clients", "-F", "#{client_width}x#{client_height}"
        ).stdout.decode().strip()
        if client_size != "%dx%d" % (width, height):
            fail("client attached at %s instead of %dx%d" %
                (client_size, width, height))

        for pane in paths.values():
            open(pane["preopen"], "w", encoding="utf-8").close()
        wait_for(
            lambda: all(os.path.exists(paths[name]["preopened"])
                for name in paths),
            2,
            "applications did not open synchronized pre-resize frames",
        )
        wait_for(
            lambda: b"LEFT_OPEN_" in run(
                "capture-pane", "-p", "-t", left
            ).stdout and b"RIGHT_OPEN_" in run(
                "capture-pane", "-p", "-t", right
            ).stdout,
            2,
            "open synchronized frames did not reach the backing screens",
        )
        drain_until_quiet(fd, 0.05, 1)

        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()

        before_widths = (
            int(run("display-message", "-p", "-t", left,
                "#{pane_width}").stdout),
            int(run("display-message", "-p", "-t", right,
                "#{pane_width}").stdout),
        )
        resize_started = time.monotonic()
        run("resize-pane", "-t", left, "-R", "10")
        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            2,
            "both applications did not receive SIGWINCH",
        )

        for pane in paths.values():
            open(pane["preclose"], "w", encoding="utf-8").close()
        wait_for(
            lambda: all(os.path.exists(paths[name]["preclosed"])
                for name in paths),
            0.05,
            "applications did not close their pre-resize synchronized frames",
        )

        before_release = read_available(fd, 0.03)
        if before_release:
            fail("client received %d bytes before applications repainted" %
                len(before_release))

        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "left application did not finish its synchronized repaint",
        )
        after_one = read_available(fd, 0.02)
        if b"LEFT_AFTER1_00_" in after_one:
            fail("left post-resize frame reached the client before every "
                "application repainted")
        if b"RIGHT_AFTER1_00_" in after_one:
            fail("right post-resize frame reached the client before it "
                "repainted")
        if after_one:
            fail("client emitted %d bytes before every application repainted: "
                "%s" % (len(after_one), after_one.hex()))

        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "right application did not finish its synchronized repaint",
        )
        first = read_before(
            fd,
            resize_started + prompt_release_deadline,
            "completed synchronized repaint was not released before %.0fms" %
                (prompt_release_deadline * 1000),
        )
        release_elapsed = time.monotonic() - resize_started
        final = first + drain_until_quiet(fd, release_quiet, 2)

        if b"LEFT_AFTER1_00_" not in final:
            client_size = run(
                "list-clients", "-F", "#{client_width}x#{client_height}"
            ).stdout.decode().strip()
            fail("final redraw does not contain the left post-resize frame "
                "(before=%d, after-one=%d, final=%d, left-before=%s, "
                "right-before=%s, right-after=%s, sync=%d/%d, client=%s)" %
                (len(before_release), len(after_one), len(final),
                b"LEFT_BEFORE_00_" in final,
                b"RIGHT_BEFORE_00_" in final,
                b"RIGHT_AFTER1_00_" in final,
                final.count(sync_start), final.count(sync_end), client_size))
        if b"RIGHT_AFTER1_00_" not in final:
            fail("final redraw does not contain the right post-resize frame")
        if final.count(sync_start) != 1 or final.count(sync_end) != 1:
            fail("final redraw was not one synchronized client transaction: "
                "%d starts, %d ends" %
                (final.count(sync_start), final.count(sync_end)))
        if release_elapsed >= prompt_release_deadline:
            fail("completed synchronized repaint released too late at %.3fs" %
                release_elapsed)

        later = read_available(fd, 0.2)
        if later:
            fail("client received a late corrective redraw of %d bytes" %
                len(later))

        after_widths = (
            int(run("display-message", "-p", "-t", left,
                "#{pane_width}").stdout),
            int(run("display-message", "-p", "-t", right,
                "#{pane_width}").stdout),
        )
        if after_widths != (before_widths[0] + 10, before_widths[1] - 10):
            fail("unexpected final widths: %r -> %r" %
                (before_widths, after_widths))

        for pane in paths.values():
            for key in ("resized", "gate", "painted"):
                os.unlink(pane[key])
            for key in ("preopened", "preclosed"):
                if os.path.exists(pane[key]):
                    os.unlink(pane[key])
            open(pane["preopen"], "w", encoding="utf-8").close()

        wait_for(
            lambda: all(os.path.exists(paths[name]["preopened"])
                for name in paths),
            0.05,
            "applications did not reopen synchronized pre-resize frames",
        )
        drain_until_quiet(fd, 0.05, 1)

        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
            open(pane["gate"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["partial"], "w", encoding="utf-8").close()

        run("resize-pane", "-t", left, "-L", "10")
        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            2,
            "both applications did not receive the spanning-frame SIGWINCH",
        )
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["partial_started"]),
            0.05,
            "left application did not start its spanning synchronized frame",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "right application did not finish its spanning-frame repaint",
        )

        spanning_early = read_available(fd, barrier_deadline + 0.07)
        if spanning_early:
            fail("client received %d bytes while a post-resize synchronized "
                "frame spanned the 150ms soft deadline" %
                len(spanning_early))

        open(paths["LEFT"]["partial_finish"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "left application did not finish its spanning synchronized frame",
        )
        spanning_final = drain_until_quiet(fd, release_quiet, 2)
        if b"LEFT_AFTER2_TOP_00_" not in spanning_final:
            fail("spanning redraw lost the top half of the synchronized frame")
        if b"LEFT_AFTER2_BOTTOM_" not in spanning_final:
            fail("spanning redraw lost the bottom half of the "
                "synchronized frame")
        if b"RIGHT_AFTER2_00_" not in spanning_final:
            fail("spanning redraw lost the completed right frame")
        if (spanning_final.count(sync_start) != 1 or
                spanning_final.count(sync_end) != 1):
            fail("spanning redraw was not one synchronized client transaction: "
                "%d starts, %d ends" %
                (spanning_final.count(sync_start),
                spanning_final.count(sync_end)))
        spanning_late = read_available(fd, 0.2)
        if spanning_late:
            fail("spanning redraw produced a late correction of %d bytes" %
                len(spanning_late))

        spanning_widths = (
            int(run("display-message", "-p", "-t", left,
                "#{pane_width}").stdout),
            int(run("display-message", "-p", "-t", right,
                "#{pane_width}").stdout),
        )
        if spanning_widths != before_widths:
            fail("unexpected spanning-frame widths: %r -> %r" %
                (after_widths, spanning_widths))

        respawn_emitters()
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
            open(pane["gate"], "w", encoding="utf-8").close()
            open(pane["skip"], "w", encoding="utf-8").close()

        no_start_at = time.monotonic()
        run("resize-pane", "-t", left, "-R", "10")
        wait_for(
            lambda: all(os.path.exists(paths[name]["painted"])
                for name in paths),
            0.05,
            "no-start applications did not acknowledge SIGWINCH",
        )
        no_start_early = read_available(
            fd, max(0, no_start_at + 0.12 - time.monotonic()))
        if no_start_early:
            fail("capable panes that never started released before the "
                "150ms soft deadline")
        no_start_first = read_before(
            fd,
            no_start_at + 0.5,
            "capable panes that never started waited for the hard deadline",
        )
        no_start_elapsed = time.monotonic() - no_start_at
        no_start_final = no_start_first + drain_until_quiet(fd, 0.2, 2)
        if no_start_elapsed < 0.12 or no_start_elapsed >= 0.5:
            fail("no-start soft release occurred at %.3fs" % no_start_elapsed)
        if (no_start_final.count(sync_start) != 1 or
                no_start_final.count(sync_end) != 1):
            fail("no-start release was not one synchronized transaction")

        respawn_emitters()
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
            open(pane["gate"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["repeat"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["skip"], "w", encoding="utf-8").close()

        hard_at = time.monotonic()
        run("resize-pane", "-t", left, "-L", "10")
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["repeat_started"]),
            0.05,
            "repeated synchronized frame did not start",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "hard-fallback companion pane did not acknowledge SIGWINCH",
        )
        hard_early = read_available(
            fd, max(0, hard_at + 0.22 - time.monotonic()))
        if hard_early:
            fail("repeated DECSET exposed a frame at the soft deadline")
        hard_first = read_before(
            fd,
            hard_at + 1.6,
            "repeated DECSET postponed the fixed hard fallback",
        )
        hard_elapsed = time.monotonic() - hard_at
        open(paths["LEFT"]["repeat_stop"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.5,
            "repeated synchronized frame did not stop",
        )
        hard_final = hard_first + drain_until_quiet(fd, 0.2, 2)
        if hard_elapsed < 1.0 or hard_elapsed >= 1.6:
            fail("hard fallback occurred at %.3fs" % hard_elapsed)
        if b"LEFT_AFTER1_REPEAT_" not in hard_final:
            fail("hard fallback redraw lost the open synchronized frame")
        if (hard_final.count(sync_start) != 1 or
                hard_final.count(sync_end) != 1):
            fail("hard fallback was not one synchronized transaction")
        if read_available(fd, 0.2):
            fail("hard fallback produced a correction after the later DECRST")

        respawn_emitters()
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
            open(pane["gate"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["partial"], "w", encoding="utf-8").close()

        run("resize-pane", "-t", left, "-R", "10")
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["partial_started"]),
            0.05,
            "resize A synchronized frame did not start",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "resize A peer frame did not finish",
        )
        resize_a_early = read_available(fd, 0.03)
        if resize_a_early:
            fail("resize A emitted %d bytes before resize B: %s" %
                (len(resize_a_early), resize_a_early.hex()))

        for pane in paths.values():
            for key in ("gate", "resized", "painted"):
                if os.path.exists(pane[key]):
                    os.unlink(pane[key])
        os.unlink(paths["LEFT"]["partial_started"])

        run("resize-pane", "-t", left, "-L", "5")
        open(paths["LEFT"]["partial_finish"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "resize A delayed DECRST was not emitted",
        )
        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            0.05,
            "resize B notification did not supersede resize A",
        )
        if read_available(fd, 0.03):
            fail("resize A delayed DECRST released resize B")

        os.unlink(paths["LEFT"]["painted"])
        for pane in paths.values():
            open(pane["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["partial_started"]),
            0.05,
            "resize B synchronized frame did not start",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "resize B peer frame did not finish",
        )
        repeated_early = read_available(fd, barrier_deadline + 0.07)
        if repeated_early:
            fail("resize A state or timer released open resize B frame")

        open(paths["LEFT"]["partial_finish"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "resize B synchronized frame did not finish",
        )
        repeated_final = drain_until_quiet(fd, release_quiet, 2)
        if b"LEFT_AFTER2_TOP_00_" not in repeated_final:
            fail("repeated-resize redraw lost resize B top rows")
        if b"LEFT_AFTER2_BOTTOM_" not in repeated_final:
            fail("repeated-resize redraw lost resize B bottom rows")
        if b"RIGHT_AFTER2_00_" not in repeated_final:
            fail("repeated-resize redraw lost resize B peer rows")
        if (repeated_final.count(sync_start) != 1 or
                repeated_final.count(sync_end) != 1):
            fail("repeated resize was not one synchronized transaction")

        respawn_emitters()
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
            open(pane["gate"], "w", encoding="utf-8").close()
            open(pane["skip"], "w", encoding="utf-8").close()
        original_width = int(run(
            "display-message", "-p", "-t", left, "#{pane_width}"
        ).stdout)
        run(
            "resize-pane", "-t", left, "-x", str(original_width + 5),
            ";",
            "resize-pane", "-t", left, "-x", str(original_width),
        )

        def left_winch_events():
            if not os.path.exists(paths["LEFT"]["winches"]):
                return []
            with open(paths["LEFT"]["winches"], encoding="utf-8") as handle:
                return [
                    (float(timestamp), int(columns), int(lines))
                    for timestamp, columns, lines in
                    (value.split() for value in handle if value.strip())
                ]

        wait_for(
            lambda: len(left_winch_events()) >= 2,
            0.5,
            "A-B-A resize did not deliver two distinct SIGWINCH callbacks",
        )
        winch_events = left_winch_events()
        winch_gap = winch_events[1][0] - winch_events[0][0]
        if winch_gap < 0.005:
            fail("A-B-A resize callbacks were only %.3fms apart" %
                (winch_gap * 1000))
        if winch_events[0][1:] == winch_events[1][1:]:
            fail("A-B-A resize callbacks observed the same geometry %dx%d" %
                winch_events[0][1:])
        if winch_events[1][1] != original_width:
            fail("A-B-A resize ended at width %d instead of %d" %
                (winch_events[1][1], original_width))
        drain_until_quiet(fd, 0.2, 2)

        respawn_emitters()
        run("set-option", "-p", "-t", left, "remain-on-exit", "on")
        run("set-option", "-p", "-t", left, "remain-on-exit-format", "")
        drain_until_quiet(fd, 0.2, 5)
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["exit_frame"], "w", encoding="utf-8").close()

        run("resize-pane", "-t", left, "-R", "10")
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["exit_started"]),
            0.05,
            "exiting synchronized frame did not start",
        )
        wait_for(
            lambda: run(
                "display-message", "-p", "-t", left, "#{pane_dead}"
            ).stdout.strip() == b"1",
            0.05,
            "remain-on-exit pane did not become dead",
        )
        exit_early = read_available(fd, 0.03)
        if exit_early:
            fail("client output escaped before the pane-exit companion "
                "completed")

        exit_at = time.monotonic()
        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "pane-exit companion frame did not finish",
        )
        exit_first = read_before(
            fd,
            exit_at + prompt_release_deadline,
            "dead synchronized pane held the barrier to the soft deadline",
        )
        exit_elapsed = time.monotonic() - exit_at
        exit_final = exit_first + drain_until_quiet(fd, release_quiet, 2)
        if exit_elapsed >= prompt_release_deadline:
            fail("pane-exit barrier released too late at %.3fs" % exit_elapsed)
        if b"LEFT_AFTER1_EXIT_" not in exit_final:
            fail("pane-exit redraw lost the dead producer's final state")
        if b"RIGHT_AFTER1_00_" not in exit_final:
            fail("pane-exit redraw lost the surviving participant's frame")
        exit_starts = exit_final.count(sync_start)
        exit_ends = exit_final.count(sync_end)
        if exit_starts != 1 or exit_ends != 1:
            fail("pane-exit redraw was not one synchronized transaction")

        respawn_emitters(False, False)
        open(paths["LEFT"]["preopen"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["preopened"]),
            0.05,
            "primary-screen application did not open its pre-resize frame",
        )
        wait_for(
            lambda: b"LEFT_OPEN_" in run(
                "capture-pane", "-p", "-t", left
            ).stdout,
            0.05,
            "primary-screen pre-resize frame did not reach the backing screen",
        )
        drain_until_quiet(fd, 0.05, 1)

        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        run("resize-pane", "-t", left, "-L", "10")
        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            0.05,
            "primary-screen applications did not receive SIGWINCH",
        )

        open(paths["LEFT"]["preclose"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["preclosed"]),
            0.05,
            "primary-screen application did not close its old frame",
        )
        primary_old = read_available(fd, 0.03)
        if primary_old:
            fail("primary-screen old DECRST released %d bytes" %
                len(primary_old))

        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "primary-screen plain peer did not repaint",
        )
        primary_peer = read_available(fd, 0.03)
        if primary_peer:
            fail("primary-screen plain peer escaped the barrier")

        primary_release_at = time.monotonic()
        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "primary-screen application did not finish its fresh frame",
        )
        primary_first = read_before(
            fd,
            primary_release_at + prompt_release_deadline,
            "primary-screen fresh frame did not release promptly",
        )
        primary_final = primary_first + drain_until_quiet(fd, release_quiet, 2)
        if b"LEFT_AFTER1_00_" not in primary_final:
            fail("primary-screen redraw lost the fresh synchronized frame")
        if b"RIGHT_AFTER1_00_" not in primary_final:
            fail("primary-screen redraw lost the plain peer frame")
        if (primary_final.count(sync_start) != 1 or
                primary_final.count(sync_end) != 1):
            fail("primary-screen redraw was not one synchronized transaction")
        if read_available(fd, 0.2):
            fail("primary-screen redraw produced a late correction")

        respawn_emitters(False)
        run("bind-key", "-n", "C-]", "resize-pane", "-t", left, "-R", "10")
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["partial"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["skip"], "w", encoding="utf-8").close()

        server_pid = int(run(
            "display-message", "-p", "#{pid}"
        ).stdout.decode())
        drain_until_quiet(fd, 0.05, 1)
        os.kill(server_pid, signal.SIGSTOP)
        try:
            open(paths["LEFT"]["same_batch"], "w", encoding="utf-8").close()
            wait_for(
                lambda: os.path.exists(paths["LEFT"]["same_batch_written"]),
                0.5,
                "same-batch pane output was not queued while server stopped",
            )
            os.write(fd, b"\035")
            time.sleep(0.02)
        finally:
            os.kill(server_pid, signal.SIGCONT)

        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            0.5,
            "same-batch resize did not reach both panes",
        )
        race_output = bytearray()
        race_deadline = time.monotonic() + 0.5
        while sync_start not in race_output:
            if time.monotonic() >= race_deadline:
                fail("same-batch pane output did not reach the client")
            race_output.extend(read_available(fd, 0.002))
        balance_deadline = time.monotonic() + 0.05
        while (race_output.count(sync_start) -
                race_output.count(sync_end)) != 0:
            if time.monotonic() >= balance_deadline:
                fail("resize barrier left a pre-resize synchronized update "
                    "open: %d starts, %d ends" %
                    (race_output.count(sync_start),
                        race_output.count(sync_end)))
            race_output.extend(read_available(fd, 0.002))
        if os.path.exists(paths["LEFT"]["painted"]):
            fail("same-batch resize frame finished before the barrier was "
                "observed")

        for pane in paths.values():
            open(pane["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["partial_started"]),
            0.5,
            "same-batch resize frame did not start",
        )
        open(paths["LEFT"]["partial_finish"], "w", encoding="utf-8").close()
        wait_for(
            lambda: all(os.path.exists(paths[name]["painted"])
                for name in paths),
            0.5,
            "same-batch resize frames did not finish",
        )
        race_final = race_output + drain_until_quiet(fd, release_quiet, 2)
        if b"LEFT_AFTER1_BOTTOM_" not in race_final:
            fail("same-batch resize lost the completed frame")
        if race_final.count(sync_start) != race_final.count(sync_end):
            fail("same-batch resize left a client transaction open")
        run("unbind-key", "-n", "C-]")
        run("resize-pane", "-t", left, "-L", "10")
        drain_until_quiet(fd, release_quiet, 3)

        respawn_emitters(False)
        open(paths["RIGHT"]["flood"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["flood_started"]),
            0.5,
            "plain peer did not begin the backpressure stream",
        )
        time.sleep(0.05)

        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["partial"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["skip"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()

        soft_race_at = time.monotonic()
        run("resize-pane", "-t", left, "-R", "10")
        time.sleep(max(0, soft_race_at + barrier_deadline + 0.05 -
            time.monotonic()))

        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["partial_started"]),
            0.5,
            "late synchronized frame did not start under backpressure",
        )
        wait_for(
            lambda: b"LEFT_AFTER1_TOP_00_" in run(
                "capture-pane", "-p", "-t", left
            ).stdout,
            0.5,
            "late synchronized frame did not reach the backing screen",
        )

        open(paths["RIGHT"]["flood_stop"], "w", encoding="utf-8").close()
        backlog = bytearray()
        backlog_deadline = time.monotonic() + 5
        while not os.path.exists(paths["RIGHT"]["flood_done"]):
            if time.monotonic() >= backlog_deadline:
                fail("backpressure stream did not stop while draining")
            readable, _, _ = select.select([fd], [], [], 0.005)
            if fd not in readable:
                continue
            try:
                backlog.extend(os.read(fd, 1 << 20))
            except (BlockingIOError, OSError):
                pass
        backlog.extend(drain_until_quiet(fd, 0.05, 2))
        if b"LEFT_AFTER1_TOP_00_" in backlog:
            fail("soft-expired WAIT_START committed after a late DECSET")
        if read_available(fd, 0.05):
            fail("late synchronized frame did not re-block redraw")

        open(paths["LEFT"]["partial_finish"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.5,
            "late synchronized frame did not finish after backpressure",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.5,
            "backpressure peer did not acknowledge its resize",
        )
        soft_race_finish = time.monotonic()
        soft_race_first = read_before(
            fd,
            soft_race_finish + prompt_release_deadline,
            "late synchronized frame did not release after DECRST",
        )
        soft_race_final = soft_race_first + drain_until_quiet(
            fd, release_quiet, 2)
        if b"LEFT_AFTER1_TOP_00_" not in soft_race_final:
            fail("soft-to-redraw race lost the top half of the frame")
        if b"LEFT_AFTER1_BOTTOM_" not in soft_race_final:
            fail("soft-to-redraw race lost the bottom half of the frame")
        if (soft_race_final.count(sync_start) != 1 or
                soft_race_final.count(sync_end) != 1):
            fail("soft-to-redraw race was not one synchronized transaction")
        if read_available(fd, 0.2):
            fail("soft-to-redraw race produced a late correction")

        for pane in paths.values():
            for path in pane.values():
                if os.path.exists(path):
                    os.unlink(path)
        run("respawn-pane", "-k", "-t", left, command("LEFT"))
        run("respawn-pane", "-k", "-t", right, command("RIGHT", False))
        wait_for(
            lambda: all(os.path.exists(paths[name]["ready"])
                for name in paths),
            5,
            "mixed-mode applications did not become ready",
        )
        wait_for(
            lambda: b"LEFT_BEFORE_00_" in run(
                "capture-pane", "-p", "-t", left
            ).stdout and b"RIGHT_BEFORE_00_" in run(
                "capture-pane", "-p", "-t", right
            ).stdout,
            5,
            "mixed-mode initial frames did not reach both backing screens",
        )
        drain_until_quiet(fd, 0.2, 5)
        time.sleep(0.3)
        drain_until_quiet(fd, 0.05, 1)

        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        run("resize-pane", "-t", left, "-R", "10")
        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            2,
            "mixed-mode applications did not receive SIGWINCH",
        )

        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "plain application did not finish its repaint",
        )
        mixed_early = read_available(fd, 0.03)
        if b"RIGHT_AFTER1_00_" in mixed_early:
            fail("plain application frame escaped the active resize barrier")
        if mixed_early:
            fail("client emitted %d bytes before the synchronized repaint: %s" %
                (len(mixed_early), mixed_early.hex()))

        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "synchronized application did not finish its mixed-mode repaint",
        )
        mixed_final = drain_until_quiet(fd, release_quiet, 2)
        if b"LEFT_AFTER1_00_" not in mixed_final:
            fail("mixed-mode redraw lost the synchronized application frame")
        if b"RIGHT_AFTER1_00_" not in mixed_final:
            fail("mixed-mode redraw lost the plain application frame")
        if (mixed_final.count(sync_start) != 1 or
                mixed_final.count(sync_end) != 1):
            fail("mixed-mode redraw was not one synchronized transaction: "
                "%d starts, %d ends" %
                (mixed_final.count(sync_start), mixed_final.count(sync_end)))

        respawn_emitters(False)
        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["skip"], "w", encoding="utf-8").close()

        run("resize-pane", "-t", left, "-L", "10")
        wait_for(
            lambda: all(os.path.exists(paths[name]["resized"])
                for name in paths),
            0.05,
            "WAIT_START transfer applications did not receive SIGWINCH",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "WAIT_START transfer peer did not acknowledge its resize",
        )

        transfer_at = time.monotonic()
        moved = run(
            "break-pane", "-d", "-P", "-F", "#{pane_id}",
            "-n", "transfer-wait", "-s", left,
        ).stdout.decode().strip()
        if moved != left:
            fail("break-pane changed the transferred pane id")
        transfer_first = read_before(
            fd,
            transfer_at + prompt_release_deadline,
            "WAIT_START pane transfer did not release the source promptly",
        )
        transfer_source = transfer_first + drain_until_quiet(fd, 0.01, 0.03)
        if b"LEFT_AFTER1_" in transfer_source:
            fail("WAIT_START pane transfer exposed the moved pane's old epoch")

        run("select-window", "-t", "resize:transfer-wait")
        drain_until_quiet(fd, 0.2, 2)
        run(
            "resize-window", "-t", "resize:transfer-wait",
            "-x", "110", "-y", str(height),
        )
        if read_available(fd, 0.03):
            fail("transferred pane did not establish a fresh destination epoch")
        destination_release_at = time.monotonic()
        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "transferred pane did not paint its fresh destination frame",
        )
        destination_first = read_before(
            fd,
            destination_release_at + prompt_release_deadline,
            "fresh destination frame did not release promptly",
        )
        destination_final = destination_first + drain_until_quiet(
            fd, release_quiet, 2)
        destination_frames = set(re.findall(
            rb"LEFT_AFTER[0-9]+_00_", destination_final))
        backing_frames = set(re.findall(
            rb"LEFT_AFTER[0-9]+_00_",
            run("capture-pane", "-p", "-t", left).stdout,
        ))
        if not destination_frames:
            fail("fresh destination epoch lost the transferred pane frame")
        if destination_frames.isdisjoint(backing_frames):
            fail("fresh destination client frame did not match the backing "
                "screen: client=%r backing=%r" %
                (sorted(destination_frames), sorted(backing_frames)))

        run("join-pane", "-d", "-h", "-s", left, "-t", right)
        drain_until_quiet(fd, 0.2, 5)
        respawn_emitters(False)

        swap_target = run(
            "new-window", "-d", "-t", "resize:", "-n", "transfer-swap",
            "-P", "-F", "#{pane_id}", "sleep 1000",
        ).stdout.decode().strip()
        run(
            "split-window", "-d", "-h", "-t", swap_target,
            "sleep 1000",
        )
        swap_width = 10 + int(run(
            "display-message", "-p", "-t", left, "#{pane_width}"
        ).stdout)
        run("resize-pane", "-t", swap_target, "-x", str(swap_width))
        run("new-session", "-d", "-t", "resize", "-s", "swap-watch")
        run("select-window", "-t", "swap-watch:transfer-swap")
        watch_pid, watch_fd = attach("swap-watch")
        complete_client_startup(watch_fd, 2)
        drain_until_quiet(watch_fd, 0.2, 2)

        for pane in paths.values():
            open(pane["arm"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["partial"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["skip"], "w", encoding="utf-8").close()
        open(paths["LEFT"]["gate"], "w", encoding="utf-8").close()
        open(paths["RIGHT"]["gate"], "w", encoding="utf-8").close()

        run("resize-pane", "-t", left, "-x", str(swap_width))
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["partial_started"]),
            0.05,
            "cross-window swap frame did not enter IN_FRAME",
        )
        wait_for(
            lambda: os.path.exists(paths["RIGHT"]["painted"]),
            0.05,
            "cross-window swap peer did not acknowledge its resize",
        )
        left_width = run(
            "display-message", "-p", "-t", left, "#{pane_width}"
        ).stdout
        target_width = run(
            "display-message", "-p", "-t", swap_target, "#{pane_width}"
        ).stdout
        if left_width != target_width:
            fail("cross-window swap widths differ: left=%r target=%r" %
                (left_width, target_width))

        swap_at = time.monotonic()
        run("swap-pane", "-d", "-s", left, "-t", swap_target)
        swap_early = read_available(watch_fd, 0.1)
        if swap_early:
            fail("cross-window swap exposed %d destination bytes before the "
                "moved synchronized frame finished: %s" %
                (len(swap_early), swap_early[:80].hex()))
        swap_first = read_before(
            fd,
            swap_at + prompt_release_deadline,
            "IN_FRAME cross-window swap did not release the source promptly",
        )
        swap_source = swap_first + drain_until_quiet(fd, release_quiet, 2)
        if b"LEFT_AFTER1_TOP_00_" in swap_source:
            fail("cross-window swap exposed the moved pane's partial frame")

        open(paths["LEFT"]["partial_finish"], "w", encoding="utf-8").close()
        wait_for(
            lambda: os.path.exists(paths["LEFT"]["painted"]),
            0.05,
            "cross-window swapped frame did not finish after detach",
        )
        swap_destination_first = read_before(
            watch_fd,
            time.monotonic() + prompt_release_deadline,
            "cross-window swapped frame did not release to the destination",
        )
        swap_destination = swap_destination_first + drain_until_quiet(
            watch_fd, release_quiet, 2)
        if b"LEFT_AFTER1_TOP_" not in swap_destination:
            fail("cross-window swap lost the moved pane's top frame half")
        if b"LEFT_AFTER1_BOTTOM_" not in swap_destination:
            fail("cross-window swap lost the moved pane's completed frame")
        if (swap_destination.count(sync_start) != 1 or
                swap_destination.count(sync_end) != 1):
            fail("cross-window swap destination was not one synchronized "
                "transaction")
    finally:
        close_client(watch_pid, watch_fd)
        cleanup(pid, fd)
PY
