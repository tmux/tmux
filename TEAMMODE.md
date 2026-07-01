# Team Mode (Multi-User Cursors)

This build of tmux adds **team mode**, which lets multiple users attached to the
same set of windows each work in their own pane at the same time and see each
other's cursors.

**Team mode is enabled by default** (the `teammode` session option defaults to
`on`).

## What it does

When `teammode` is on:

1. **Independent active pane per user.** Each attached client controls its own
   active pane (via tmux's existing `active-pane` client flag, which the option
   sets automatically on attach). Keystrokes from each user go to *their* pane,
   and each user's terminal cursor sits in *their* pane.

2. **Visible remote cursors.** Each client draws a coloured marker at the cursor
   position of every *other* client viewing the same window. Every user is
   assigned a distinct colour so you can tell collaborators apart. Markers are
   transient — painted over the pane content on each redraw and never stored in
   the screen.

3. **Independent window navigation** (via session groups). Each user attaches to
   their own session in a *session group*; the sessions share the same windows
   and panes (same processes) but keep an independent "current window", so users
   can move between windows independently.

### Design constraints

- A pane runs a single program with a single cursor, so independent cursors only
  exist **across different panes**, never within one pane.
- The marker colour is derived from the client name, so a given user keeps the
  same colour across detach/reattach.

## Does `Ctrl+b c` (new-window) give each user their own window?

**Only if each user is on their own session in a group.** It depends on the
workflow:

- **Same session** (`tmux new -s foo` then `tmux a -t foo`): both clients share
  one session, so they share the *current window*. When either user presses
  `Ctrl+b c` (or switches windows), **both** terminals jump to the new/other
  window. Team mode still gives independent panes and remote cursors — only
  window *navigation* is linked.
- **Grouped sessions** (`tmux new -s foo` then `tmux new -s foo2 -t foo`): the
  sessions share windows/panes but each has its own current window, so
  `Ctrl+b c` and window switching are **independent** per user.

## Changes in this build

| File | Change |
|------|--------|
| `options-table.c` | New session option `teammode` (flag, **default on**). |
| `server-client.c` | Auto-set `CLIENT_ACTIVEPANE` on attach when `teammode` is on; assign a stable per-user marker colour (`server_client_team_colour`); detect cursor-only moves and redraw observers (`server_client_window_teammode`, in `server_client_loop`). |
| `screen-redraw.c` | New `redraw_draw_remote_cursors()`, called on full-window redraws, paints each other client's cursor as a coloured cell. |
| `tmux.h` | `struct client.team_colour`; `struct window_pane.lastcx/lastcy`. |
| `tmux.1` | Documentation for the `teammode` option and workflow. |

Total: ~187 added lines across 5 files.

### How it works internally

- Pane selection, input routing, and per-client cursor placement already support
  a per-client active pane through the pre-existing `CLIENT_ACTIVEPANE` flag
  (`server_client_get_pane`). The option simply turns this flag on automatically.
- `redraw_draw_remote_cursors()` iterates the client list; for each other client
  on the same window it reads that client's active-pane cursor
  (`server_client_get_pane` → `screen->cx/cy`), converts to the observing
  client's viewport coordinates (same math as `server_client_reset_state`), and
  writes a restyled cell with `tty_cell()`.
- A bare cursor move (e.g. arrow keys in an editor) does not normally trigger a
  redraw. `server_client_loop()` detects such moves in team-mode windows
  (comparing each pane's cursor to cached `lastcx/lastcy`) and forces a redraw of
  the observers so markers stay current.

## Step-by-step usage

### 1. Build (if not already built)

```sh
cd /shared/tmux
sh autogen.sh
./configure
make
```

The binary is `/shared/tmux/tmux`. Use it directly as `./tmux` or install it with
`make install` (needs sudo; default location `/usr/local/bin/tmux`).

### 2. Same window, different panes (simplest — nothing to enable)

Team mode is on by default, so just attach two clients to one session:

```sh
# User A – create the session and split into panes:
./tmux new-session -s work
#   (inside tmux: prefix + %  or  prefix + "  to create panes)

# User B – attach to the same session (in another terminal):
./tmux attach -t work
```

Each user selects a different pane (`prefix + arrow`, `prefix + o`, or click) and
types independently. Each user sees the other's cursor as a coloured block.

> Note: on a shared session, `Ctrl+b c` / window switching affects both users
> (see the section above). Use grouped sessions for independent navigation.

### 3. Independent window navigation (session groups)

Give each user their own session in a group so they can move between windows
independently while sharing the same windows/panes:

```sh
# User A – create the session:
./tmux new-session -s work

# User B – create a grouped session sharing A's windows, and attach:
./tmux new-session -t work
```

`new-session -t work` creates a session linked to the same windows/panes and
attaches to it in one step. Both users now:

- share the same windows and the programs running in them;
- switch windows independently (`prefix + n/p/<number>`, `prefix + c`);
- keep independent active panes and see each other's cursors.

Setting `aggressive-resize on` is recommended so a window is sized only by the
users currently viewing it:

```sh
./tmux set-option -g aggressive-resize on
```

### 4. Turning team mode off

It is on by default. To disable it (globally, in `~/.tmux.conf` or at runtime):

```
set -g teammode off
```

The option takes effect for a client **when it attaches**, so change it before
attaching (a config setting does this automatically). To change it for an
already-attached client, detach and reattach.

### 5. Verify

```sh
./tmux show-options -g teammode                       # -> teammode on
./tmux list-clients -F '#{client_name} [#{client_flags}]'
#   each attached client line should include: active-pane
```

## Notes and limitations

- **Marker refresh cost.** Cursor-only moves currently trigger a full-window
  redraw of observers. Correct but potentially heavy if a program animates the
  cursor rapidly in a shared window; a lighter cursor-only redraw path is a
  possible future optimization.
- **One cursor per pane.** Two users cannot have separate cursors in the *same*
  pane. Use different panes.
- **Read-only / control clients** do not draw markers.
