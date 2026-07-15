#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════════
# Deterministic reproducer for the tty_draw_line livelock caused by a
# non-padding, zero-width grid cell.
#
# Context
#   Upstream tmux commit 281e8ff7 (2026-05-13) fixed the #5024 livelock but
#   only for GRID_FLAG_PADDING (orphan padding) cells: those now return
#   empty=1 from tty_draw_line_get_empty() and the draw loop advances by 1.
#
#   A *non-padding* cell with utf8_data.width == 0 (e.g. a stray variation
#   selector U+FE0F, a combining mark, or any zero-width code point that the
#   grid did not fold into the preceding cell) is NOT classified empty: it
#   falls through every clause in tty_draw_line_get_empty() and the draw loop
#   then executes `i += gcp->data.width;` with width == 0. i never advances,
#   the for(;;) never terminates, and the tmux server livelocks on one core
#   (state Rs). The only recovery is kill -9 on the server PID.
#
#   This reproducer plants that exact pathological cell directly into the live
#   server grid via gdb, then forces a redraw. There is no timing window: an
#   unpatched server livelocks every run; a patched server survives every run.
#
# Requirements
#   gdb, passwordless sudo (ptrace attach), the tmux source tree (for struct
#   offsets, auto-computed below), a built ./tmux binary.
#
# Safety
#   Runs on a private throwaway socket (-L repro-$$). The real 'default'
#   server is never touched. On livelock the server can only be recovered
#   with kill -9 — we apply that only to the private PID.
#
# Usage
#   tools/tty-draw-zero-width-livelock-repro.sh ./tmux        # RED or GREEN
#   tools/tty-draw-zero-width-livelock-repro.sh /path/to/tmux
# ══════════════════════════════════════════════════════════════════════════════
set -uo pipefail

TMUX_BIN="${1:-./tmux}"
[ -x "$TMUX_BIN" ] || { echo "not executable: $TMUX_BIN" >&2; exit 2; }

# Resolve the tmux source tree (for offsetof probe) relative to this script.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
[ -f "$SRC_DIR/tmux.h" ] || { echo "tmux.h not found in $SRC_DIR" >&2; exit 2; }

SOCK="repro-$$"
log() { printf '\033[1;34m[repro]\033[0m %s\n' "$*" >&2; }

command -v gdb >/dev/null || { echo "gdb required" >&2; exit 2; }

log "target binary: $TMUX_BIN ($("$TMUX_BIN" -V 2>&1))"
log "source tree:   $SRC_DIR"
log "private socket: $SOCK  (your 'default' server is NOT touched)"

# ── Auto-compute struct offsets for this exact build ──────────────────────────
OFFSETS_PROBE="$(mktemp -d)/offsets.c"
cat > "$OFFSETS_PROBE" <<'EOF'
#include <stddef.h>
#include <stdio.h>
#define TMUX_H_INSIDE 1
#define TMUX_NAME "__probe__"
#include "tmux.h"
int main(void) {
    printf("WP_BASE=%zu\n", offsetof(struct window_pane, base));
    printf("SCREEN_GRID=%zu\n", offsetof(struct screen, grid));
    printf("GC_SIZE=%zu\n", sizeof(struct grid_cell));
    printf("GC_DATA=%zu\n", offsetof(struct grid_cell, data.data));
    printf("GC_USIZE=%zu\n", offsetof(struct grid_cell, data.size));
    printf("GC_UWIDTH=%zu\n", offsetof(struct grid_cell, data.width));
    printf("GC_BG=%zu\n", offsetof(struct grid_cell, bg));
    return 0;
}
EOF
OFFSETS_BIN="${OFFSETS_PROBE%.c}"
if ! gcc -I"$SRC_DIR" -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 \
        -DTMUX_VERSION='"probe"' -DTMUX_TERM='"xterm"' -DTMUX_MOUSE=1 \
        -std=gnu99 -O0 "$OFFSETS_PROBE" -o "$OFFSETS_BIN" 2>/dev/null; then
    echo "failed to compile offsets probe against $SRC_DIR" >&2
    rm -rf "$(dirname "$OFFSETS_PROBE")"
    exit 2
fi
eval "$("$OFFSETS_BIN")"
rm -rf "$(dirname "$OFFSETS_PROBE")"
log "offsets: WP_BASE=$WP_BASE SCREEN_GRID=$SCREEN_GRID GC_SIZE=$GC_SIZE GC_UWIDTH=$GC_UWIDTH GC_BG=$GC_BG"

# ── 1. Fresh isolated server + attached real pty client (redraw needs a client)
"$TMUX_BIN" -L"$SOCK" -f /dev/null new-session -d -s repro -x 120 -y 30 'sleep 600'
SRVPID=$("$TMUX_BIN" -L"$SOCK" display -p '#{pid}')
script -qfc "$TMUX_BIN -L$SOCK attach -t repro" /dev/null </dev/null >/dev/null 2>&1 &
CLIENTPID=$!
sleep 2
PTS=$("$TMUX_BIN" -L"$SOCK" list-clients -F '#{client_name}' | head -1)
log "server pid=$SRVPID  client pts=$PTS"

cleanup() {
    # Livelocked server ignores tmux cmds and SIGTERM — kill -9 the private pid.
    kill -9 "$SRVPID" "$CLIENTPID" 2>/dev/null
    pkill -9 -f "$SOCK" 2>/dev/null
    rm -f "/tmp/tmux-$(id -u)/$SOCK" 2>/dev/null
}
trap cleanup EXIT

# ── 2. Plant the pathological cell at column 5, row 0:
#       U+FE0F (EF B8 8F), size=3, width=0, NON-PADDING, non-default bg.
#    This is the grid state pi/hermes/codex create by chance when a variation
#    selector is emitted standalone into a colored cell.
cat > "/tmp/repro-inject-$$.gdb" <<GDB
set confirm off
set pagination off
set var \$wp = ((void*(*)(unsigned int))window_pane_find_by_id)(0)
set var \$gd = *(void**)((char*)\$wp + $WP_BASE + $SCREEN_GRID)
set var \$gc = (char*)((void*(*)(unsigned long))malloc)($GC_SIZE)
call ((void*(*)(void*,void*,unsigned long))memcpy)(\$gc, &grid_default_cell, $GC_SIZE)
set var *(unsigned char*)(\$gc + $GC_DATA + 0) = 0xef
set var *(unsigned char*)(\$gc + $GC_DATA + 1) = 0xb8
set var *(unsigned char*)(\$gc + $GC_DATA + 2) = 0x8f
set var *(unsigned char*)(\$gc + $GC_USIZE)  = 3
set var *(unsigned char*)(\$gc + $GC_UWIDTH) = 0
set var *(int*)(\$gc + $GC_BG) = 2
call ((void(*)(void*,unsigned,unsigned,void*))grid_view_set_cell)(\$gd, 5, 0, \$gc)
printf "INJECTED width0 non-padding cell into gd=%p\n", \$gd
detach
quit
GDB
log "injecting width==0 non-padding cell via gdb (sudo)..."
sudo gdb -batch -x "/tmp/repro-inject-$$.gdb" -p "$SRVPID" 2>&1 | grep -E "INJECTED|error|Cannot|warning" | head -3
rm -f "/tmp/repro-inject-$$.gdb"

# ── 3. Force a full redraw and measure.
read -r _ _ _ _ _ _ _ _ _ _ _ _ _ ut st _ < "/proc/$SRVPID/stat"; t0=$((ut+st))
timeout 3 "$TMUX_BIN" -L"$SOCK" refresh-client -t "$PTS" >/dev/null 2>&1
refresh_rc=$?
sleep 3
if [ -r "/proc/$SRVPID/stat" ]; then
    read -r _ _ _ _ _ _ _ _ _ _ _ _ _ ut st _ < "/proc/$SRVPID/stat"; t1=$((ut+st))
    ticks=$((t1-t0))
else
    ticks="server-gone"
fi
timeout 3 "$TMUX_BIN" -L"$SOCK" display -p ok >/dev/null 2>&1
display_rc=$?
state=$(ps -o stat= -p "$SRVPID" 2>/dev/null | tr -d ' ')

echo
log "── RESULT ──────────────────────────────────────────"
log "refresh-client rc = $refresh_rc   (124 = timed out = LIVELOCK)"
log "CPU ticks in 3s   = $ticks        (>250 = pegged core = LIVELOCK)"
log "display-message rc= $display_rc   (0 = responsive)"
log "server state      = ${state:-gone} (Rs = spinning = LIVELOCK; Ss = idle = OK)"
if [ "$display_rc" -eq 0 ] && { [ "$ticks" = "server-gone" ] || [ "$ticks" -lt 100 ]; }; then
    log "VERDICT: ✅ SURVIVED — non-padding zero-width cell does not livelock"
    exit 0
else
    log "VERDICT: ❌ LIVELOCKED — non-padding zero-width cell livelocks the server"
    exit 1
fi
