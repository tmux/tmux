# PR comment placement guide

These explanations have now been adapted into normal source comments at the
locations below. The wording here is kept as a reference for GitHub review
conversations when a reviewer asks about the same decisions.

## Core implementation

### `cmd-help.c`

Place this beside the command description and example tables:

> I kept the descriptions and examples together here so command discovery has
> one source of truth. The completeness check also prevents a new command from
> being added without help and at least one example.

Place this beside `cmd_help_print`:

> This resolves aliases through the normal command lookup, so `tmux help
> splitw` and `tmux help split-window` intentionally produce the same result.

### `cmd-output.c`

Place this beside `cmd_output_is_human`:

> The readable layout is selected only for an interactive client. Redirected
> output and control mode stay on the existing machine-readable path.

Place this beside the width and wrapping code:

> Long values wrap in the final column, while earlier columns are clipped only
> when the terminal is too narrow to preserve a useful value column.

### `server-client.c`

Place this beside the `stdout_tty` initialization:

> This records the stdout TTY state and width before `out_fd` is closed during
> client setup, so one-shot commands can still choose the correct output style.

### `tmux.h`

Place this beside the output-style declarations:

> These helpers keep terminal detection, styling, wrapping, and table layout
> consistent instead of reimplementing them in every command.

### `cmd.c`

Place this beside `cmd_help_entry` in the command table:

> Registering `help` as a normal command means it gets the same alias lookup,
> parsing, and command queue behavior as the rest of tmux.

## Command output changes

### `cmd-list-commands.c`

Place this beside the human-output branch:

> Interactive `list-commands` now acts as the command catalog, while pipes and
> `-F` retain the original syntax listing for compatibility.

### `cmd-list-sessions.c`

Place this where the Sessions table is created:

> The default terminal view surfaces the fields I usually need first: session
> name, window count, attachment state, group, and creation time.

### `cmd-list-windows.c`

Place this where the human table chooses between the normal and `-a` schemas:

> `-a` includes the session column because windows from multiple sessions would
> otherwise be ambiguous.

### `cmd-list-panes.c`

Place this beside the shared table passed through the server/session/window
helpers:

> A single table is accumulated across the selected scope so `-a` and `-s`
> produce one aligned result instead of repeating headings for each window.

### `cmd-list-clients.c`

Place this where the Clients table is populated:

> The interactive view keeps identity and connection details together, while
> custom formats remain available for less common client fields.

### `cmd-list-keys.c`

Place this beside the human-output format:

> Notes are preferred when a binding has one; otherwise the action column shows
> the bound command, which makes the default table easier to scan.

### `cmd-list-buffers.c`

Place this beside the preview trimming:

> Buffer previews are intentionally bounded so a large buffer cannot dominate
> the table. The full contents are still available through `show-buffer`.

### `cmd-show-options.c`

Place this beside the condition that excludes `-F` and `-v`:

> `-F` and value-only output are often consumed by scripts, so the table is
> limited to the unformatted interactive form.

### `cmd-show-prompt-history.c`

Place this where history entries are added to the table:

> Numbering the entries makes the history easier to discuss and scan without
> changing the existing redirected output.

## Top-level behavior and documentation

### `tmux.c`

Place this beside the successful `-h` output:

> Error paths keep the short usage text; the common-command guide is shown only
> for an explicit, successful help request.

### `tmux.1`

Place this beside the new terminal-output paragraph:

> This documents the compatibility boundary explicitly: terminal defaults may
> be richer, but redirection, control mode, and `-F` remain stable.

### `CHANGES`

Place this on the new changelog entry:

> I grouped help and readable command output together because they are two
> parts of the same command-discovery improvement.

### `Makefile.am`

Place this beside the two new source entries:

> These register the shared output renderer and the new help command in the
> normal tmux build.

## Tests

### `regress/command-help.sh`

Place this beside the loop over every registered command:

> This deliberately checks every command instead of a sample, so future
> commands cannot silently omit help or examples.

Place this beside the pipe and `-F` assertions:

> These compatibility checks are the main guard against making interactive
> improvements at the expense of existing scripts.
