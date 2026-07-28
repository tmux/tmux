# Add terminal-aware command help and readable list output

## Summary

This adds a built-in `help` command and improves the default interactive output
of tmux's most commonly used list and show commands.

The new output is intended for people reading it in a terminal. Existing
line-oriented output is preserved for scripts, pipes, control mode clients, and
commands that provide an explicit `-F` format.

## Motivation

This came out of my workflow training models on GPU clusters. I use tmux to
keep training, monitoring, data transfer, and debugging sessions alive while I
disconnect and reconnect to a cluster.

Those jobs often run across multiple weeks. Each time I needed to spin up a new
cluster, I found myself having to remember or look up the same tmux commands
and flags again. The existing command list is complete, but it is difficult to
scan when I am trying to get a cluster running quickly.

I wanted command discovery to feel closer to Git: a useful top-level overview,
clear per-command usage, practical examples, and readable tables for common
status commands.

## What changed

- Add `tmux help` with a categorized overview of all commands.
- Add `tmux help <command>` with the command's purpose, usage, alias, and
  practical examples.
- Include at least one example for every registered command.
- Expand `tmux -h` with a short list of common commands.
- Add terminal-aware tables for:
  - `list-commands`
  - `list-sessions`
  - `list-windows`
  - `list-panes`
  - `list-clients`
  - `list-keys`
  - `list-buffers`
  - `show-options`, `show-window-options`, and `show-hooks`
  - `show-prompt-history`
- Wrap long usage text and table values based on terminal width.
- Add restrained ANSI styling for headings, identifiers, state, warnings, and
  errors.
- Respect `NO_COLOR` and `TERM=dumb`.

## Examples

```sh
tmux help
tmux help split-window
tmux help rename-session
tmux list-commands
tmux list-sessions
tmux list-panes
tmux show-options -g
```

Detailed help now includes an example section:

```text
NAME
  rename-session - Rename a session

USAGE
  tmux rename-session [-t target-session] new-name

ALIAS
  rename

EXAMPLES
TASK              COMMAND
Rename a session  tmux rename-session -t work project
```

## Compatibility

- Redirected and piped output keeps the existing line-oriented format.
- Control mode output is unchanged.
- An explicit `-F` format always takes precedence.
- Value-only forms such as `show-options -v` remain unchanged.
- `NO_COLOR` disables ANSI styling without disabling the readable layout.

For example, these continue to be suitable for scripts:

```sh
tmux list-sessions | while IFS= read -r session; do
    # ...
done

tmux list-panes -F '#{pane_id} #{pane_current_command}'
```

## Testing

- Built with:

  ```sh
  ./autogen.sh
  ./configure --enable-debug --disable-jemalloc
  make -j4
  ```

- Added `regress/command-help.sh`, covering:
  - all registered commands having help and examples;
  - canonical command names and aliases;
  - interactive table selection;
  - narrow terminal rendering;
  - `NO_COLOR`;
  - legacy pipe output; and
  - explicit `-F` compatibility.
- Existing buffer, option, session, window, pane, control, format, input,
  target, theme, TTY, UTF-8, and hook regressions pass.
- The full regression run has one unrelated existing-result mismatch in
  `screen-redraw-menus.sh`, where a menu over a split pane is clipped
  differently from its checked-in golden scene.

