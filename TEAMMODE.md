# Team Mode (Multi-User Independent Panes, Windows and Cursors)

This build of tmux adds **team mode**, which lets multiple users attached to the
same session each work fully independently — their own pane, their own current
window, and a view of everyone else's cursors.

**Team mode is enabled by default** (the `teammode` session option defaults to
`on`).

## What it does

When `teammode` is on, each client attached to the session is independent:

1. **Independent active pane per user.** Each client controls its own active
   pane (auto `active-pane` client flag). Keystrokes go to *that user's* pane and
   each user's terminal cursor sits in *their* pane.

2. **Independent current window per user.** Each client has its own current
   window (auto `active-window` client flag), so users can view and navigate to
   different windows of the *one* session independently. `select-window`,
   `next-window`, `previous-window`, `last-window`, `new-window` and the numeric
   window keys affect only the issuing client — `Ctrl+b c` in one terminal no
   longer drags the others to the new window.

3. **Visible remote cursors.** Each client draws a coloured marker at the cursor
   position of every *other* client viewing the same window. Every user gets a
   distinct colour. Markers are transient — painted over the pane content on each
   redraw and never stored in the screen.

When `teammode` is **off**, all clients on a session share one active pane and one
current window, as classic tmux does (useful for mirroring a session on a
projector).

### Design constraints

- A pane runs a single program with a single cursor, so independent cursors only
  exist **across different panes**, never within one pane.
- The marker colour is derived from the client name, so a user keeps the same
  colour across detach/reattach.
- `last-window` uses a single per-client "last window" pointer (not a full
  stack).

## Does `Ctrl+b c` (new-window) give each user their own window?

**Yes**, with team mode on (the default): windows are per-client within a single
session. `Ctrl+b c` creates a window and switches *only the client that pressed
it*; other users stay where they are (the new window still appears in everyone's
window list). Window switching (`Ctrl+b n/p/l/<number>`) is likewise per-client.

With `teammode off`, `Ctrl+b c` and window switches move every client on the
session together, as before.

## Recommended companion setting

Enable `aggressive-resize` so a window is sized only by the clients currently
viewing it — two users on different windows then won't shrink each other:

```
set -g aggressive-resize on
```

## Changes in this build

| File | Change |
|------|--------|
| `options-table.c` | Session option `teammode` (flag, **default on**). |
| `tmux.h` | `client.curw` / `client.last_curw`; `CLIENT_ACTIVEWINDOW` flag; `client.team_colour`; `window_pane.lastcx/lastcy`. |
| `server-client.c` | Auto-set `CLIENT_ACTIVEPANE`+`CLIENT_ACTIVEWINDOW` on attach; per-client window accessors `server_client_get_curw` / `is_viewing` / `set_curw` + next/previous/last; `server_client_remove_window`; per-user marker colour; cursor-move redraw; ~20 client-scoped `curw` reads switched to the per-client accessor. |
| `server-fn.c`, `screen-write.c`, `tty.c`, `screen-redraw.c`, `window.c`, `tty-keys.c`, `resize.c` | Client-scoped "is this client viewing window w / what window does this client see" reads switched to `server_client_is_viewing` / `server_client_get_curw`. |
| `cmd-select-window.c`, `cmd-new-window.c`, `spawn.c`, `cmd-switch-client.c` | Per-client window navigation when the issuing client has `active-window`. |
| `format.c` | Client-context formats (status line, window list, `#{window_active}`) resolve the current window per-client. |
| `session.c` | `session_detach` repoints per-client current windows off a removed winlink. |
| `tmux.1` | Documentation. |

Session-scoped uses of `session->curw` (command targeting in `cmd-find.c`,
session internals, session-level formats) are intentionally left unchanged.

## Step-by-step usage

### 1. Build

```sh
cd /shared/tmux
sh autogen.sh
./configure
make
```

The binary is `/shared/tmux/tmux`.

### 2. Two users, one session (nothing to enable)

```sh
# User A:
./tmux new-session -s work            # create some windows/panes
# User B, in another terminal:
./tmux attach -t work
```

Each user independently:
- selects a different pane and types into it;
- switches to / creates different windows (`Ctrl+b c`, `Ctrl+b n/p`, `Ctrl+b <n>`);
- sees the others' cursors as coloured blocks when viewing the same window.

### 3. Turning team mode off (restore classic shared session)

In `~/.tmux.conf` or at runtime:

```
set -g teammode off
```

The option takes effect for a client **when it attaches**, so change it before
attaching (a config setting does this automatically); to change an
already-attached client, detach and reattach.

### 4. Verify

```sh
./tmux show-options -g teammode                        # -> teammode on
./tmux list-clients -F '#{client_name} [#{client_flags}]'
#   attached clients show: active-pane, active-window
./tmux list-clients -F 'curwin=#{window_index}'        # differs per client
```

## Notes and limitations

- **Marker / view refresh cost.** A bare cursor move or window switch triggers a
  redraw of the affected observers. Correct but potentially heavy with rapid
  cursor animation in a shared window; a lighter cursor-only redraw path is a
  possible future optimization.
- **One cursor per pane.** Two users cannot have separate cursors in the same
  pane — use different panes.
- **last-window** is a single per-client pointer, not a stack.
- **Alerts / window flags** are per-session (shared by clients on a session) and
  clear when a viewing client selects the window.
- **Read-only / control clients** do not draw cursor markers.
