# Fuzzing tmux with Bombadil

[Bombadil](https://github.com/antithesishq/bombadil) is a property-based
tester for terminal programs by [Antithesis](https://antithesis.com). The
libFuzzer harnesses next door fuzz individual parsers in-process; Bombadil
drives a real tmux client in a pty with random input (keys, bindings,
commands, pastes, resizes, scrolls) and checks temporal properties against
every rendered screen.

## Running

Build tmux at the top of the tree, then:

```sh
cd fuzz/bombadil
npm install
TIME_LIMIT=5m npm test
```

`run.sh` isolates the run with a private `TMUX_TMPDIR` and `HOME` and the
config in `tmux.conf`. Extra options pass through to `bombadil terminal test`:

```sh
sh run.sh --output-path output --output-path-overwrite
TMUX_BIN=/path/to/other/tmux sh run.sh
```

Traces record the full grid for every state and grow to gigabytes per
minute, so keep `--output-path` runs short and delete traces you don't need.

To replay a violation, point `--reproduce` at the trace (gunzip it first if
it came from a CI artifact) with the same options as the original run:

```sh
sh run.sh --reproduce output
```

## What is checked

Properties live in `specification.ts`:

* `exitSuccess` (Bombadil default): the client never exits nonzero. This
  includes the client noticing a dead server, so it catches server crashes.
* `clientNeverKilledBySignal`: the client is never killed by a signal.
* `serverNeverCrashes`: the screen never shows "server exited unexpectedly".
* `statusLineRecovers`: whenever a prompt, message or mid-redraw blank
  replaces the status line, the `[bombadil]` line returns within 30 seconds.

Bombadil's other default, `noReplacementChars` (no U+FFFD on screen), is
excluded: tmux renders U+FFFD by design for invalid UTF-8 from a pane
(`input_stop_utf8()`), which random shell echo produces routinely.

The action mix weights plain Unicode and control-key input against prefix
bindings (splits, kills, layouts, copy mode, clock, choose-tree, renames),
prompt commands, bracketed pastes, scrolling and resizes.

`tmux.conf` unbinds detach, suspend and rename-session so random input can't
end the run or break the status-line property, and sets `exit-unattached` so
servers don't leak between test cases.

## CI

`.github/workflows/bombadil.yml` runs nightly and on demand
(workflow_dispatch, with a configurable number of one-minute rounds). On a
violation the gzipped trace is uploaded as an artifact; `--reproduce` it as
above to replay locally.
