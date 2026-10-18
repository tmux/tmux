# Copy-mode incremental search design

Incremental search re-runs the search on every keystroke in the prompt, so
the view follows the term as it is typed. This document describes the design
of the existing incremental search and how regular-expression support slots
into it (GitHub issue 2205).

## Prompt protocol

Incremental searches are driven through `command-prompt -i`: the template
command is fired on every edit, and the `%%` substitution receives the search
term prefixed with a single character that says what to do (see
`prompt_key()` in `prompt.c`):

- `=` -- search in the command's default direction. This is also what a bare
  `C-s`/`C-r` press emits: when the buffer is empty the prompt restores the
  previous search term, so pressing the key repeats the last search.
- `+` -- search down for the next occurrence. Emitted by `C-s` when the
  buffer is not empty.
- `-` -- search up for the previous occurrence. Emitted by `C-r` when the
  buffer is not empty.

`+` and `-` override the command's default direction, so `C-r` inside a
forward prompt searches upward with the current term. Any edit other than
`C-s`/`C-r` (typing, deleting, word movements) leaves the prefix at `=`. An
empty term (just `=`) clears the search marks.

The default key bindings (`key-bindings.c`) use this for the emacs-style
`C-r`/`C-s` bindings in copy mode:

	bind -Tcopy-mode C-s {
		command-prompt -P -T search -ip'(search down)' \
			-I'#{pane_search_string}' \
			{ send -X search-forward-incremental -- '%%' } }

## Copy-mode side

`window-copy.c` keeps the search state in `struct window_copy_mode_data`:

- `searchstr`, `searchtype`, `searchregex` -- the current term, direction
  and whether it is a regular expression; `search-again` and
  `search-reverse` replay these.
- `searchx`, `searchy`, `searcho` -- the cursor position before the current
  term matched; the incremental handler restores the cursor (and therefore
  the view) whenever the term changes, so shortening the term walks back to
  the previous match.
- search marks carry the match positions and counts, exposed to formats as
  `search_present`, `search_count`, `search_count_current` and
  `search_match`.

`window_copy_search()` honours `wrap-search`, matches case-insensitively
when the term is all lowercase, and follows `mode-keys`: emacs leaves the
cursor after the match, vi at its start, which is why an emacs search can
find a match under the cursor where a vi search moves on.

## Regular-expression variants

`search-forward-incremental-regex` and
`search-backward-incremental-regex` reuse all of the above with
`searchregex` set. Two existing behaviours make this safe mid-edit:

- A term with no regular-expression metacharacters is demoted to a plain
  text match (`window_copy_search()`), so typing an ordinary string still
  behaves exactly like the plain-text search.
- An invalid expression simply fails to compile and is reported as
  "no match", so a half-typed term such as `foo[` does not error -- it
  matches again once it becomes valid.

The match position cache is keyed on the term *and* whether it is a regex,
so switching between the regex and plain-text commands rescores every mark
rather than reusing stale positions. No default keys are bound for the new
commands; bind them like the emacs defaults, for example:

	bind -Tcopy-mode C-s {
		command-prompt -P -T search -ip'(search)' \
			-I'#{pane_search_string}' \
			{ send -X search-forward-incremental-regex -- '%%' } }
