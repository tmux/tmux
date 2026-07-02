// Bombadil property-based fuzzing specification for tmux.
//
// Bombadil (https://github.com/antithesishq/bombadil) drives a tmux client
// in a pty with randomly generated input and checks temporal properties
// against the rendered screen. Run it with run.sh in this directory; the
// properties below assume the configuration in tmux.conf (session named
// "bombadil", detach and session rename unbound).
import { always, eventually, now } from "@antithesishq/bombadil";
import { actions, extract, weighted } from "@antithesishq/bombadil/terminal";
import {
	CharSet,
	CharSets,
	typeFromSet,
} from "@antithesishq/bombadil/terminal/defaults/actions";

// Default property: the client never exits nonzero. The other default,
// noReplacementChars, is not exported: tmux renders U+FFFD for invalid
// UTF-8 from a pane (input_stop_utf8() in input.c), which random typing
// into the shell produces routinely.
export { exitSuccess } from "@antithesishq/bombadil/terminal/defaults/properties";

/* Extractors. */

const running = extract((state) => state.exitStatus === null);

const exitSignal = extract((state) => {
	return state.exitStatus === null ? null : state.exitStatus.signal;
});

// The bottom row: status line, or a prompt or message while one is active.
const statusLine = extract((state) => {
	const rows = state.grid.size.rows;
	return rows > 0 ? state.grid.rowText(rows - 1) : "";
});

// The client prints this before exiting when the server dies underneath it.
const serverCrashText = extract((state) => {
	for (let i = 0; i < state.grid.size.rows; i++) {
		const text = state.grid.rowText(i);
		if (text.includes("server exited unexpectedly")) {
			return text;
		}
	}
	return null;
});

/* Properties. */

// The client must never die from a signal (SIGSEGV, SIGABRT, ...). Clean
// exits (killing the last pane, kill-server) exit 0 and are fine.
export const clientNeverKilledBySignal = always(
	() => exitSignal.current === null,
);

// The server must never crash: on crash the client prints "server exited
// unexpectedly". Random shell echo cannot plausibly reproduce that string.
export const serverNeverCrashes = always(
	() => serverCrashText.current === null,
);

// Prompts, messages and mid-redraw sampling can replace or blank the status
// line, but it must always come back to the "[bombadil]" session name.
// tmux.conf keeps this reachable (detach and rename unbound, short
// display-time); Enter is generated often, so 30 seconds is generous.
export const statusLineRecovers = always(
	now(
		() => running.current && !statusLine.current.startsWith("[bombadil]"),
	).implies(
		eventually(
			() =>
				!running.current ||
				statusLine.current.startsWith("[bombadil]"),
		).within(30, "seconds"),
	),
);

/* Actions. */

// Default prefix key.
const PREFIX = "\x02";

// Prefix-key bindings, exercising window and pane management. Detach (d) and
// rename-session ($) are unbound in tmux.conf and omitted here too.
export const prefixCommands = typeFromSet(
	CharSet.fromLiterals(
		PREFIX + "c", // new-window
		PREFIX + "n", // next-window
		PREFIX + "p", // previous-window
		PREFIX + "l", // last-window
		PREFIX + "0",
		PREFIX + "1",
		PREFIX + "2",
		PREFIX + '"', // split-window
		PREFIX + "%", // split-window -h
		PREFIX + "o", // select-pane -t :.+
		PREFIX + ";", // last-pane
		PREFIX + "\x1b[A", // select-pane -U
		PREFIX + "\x1b[B", // select-pane -D
		PREFIX + "\x1b[C", // select-pane -R
		PREFIX + "\x1b[D", // select-pane -L
		PREFIX + "x", // kill-pane (confirm)
		PREFIX + "&", // kill-window (confirm)
		PREFIX + "z", // resize-pane -Z (zoom)
		PREFIX + " ", // next-layout
		PREFIX + "!", // break-pane
		PREFIX + "{", // swap-pane -U
		PREFIX + "}", // swap-pane -D
		PREFIX + "q", // display-panes
		PREFIX + "t", // clock-mode
		PREFIX + "w", // choose-tree
		PREFIX + "[", // copy-mode
		PREFIX + "]", // paste-buffer
		PREFIX + ",", // rename-window (prompt; random input renames and confirms)
		PREFIX + "m", // select-pane -m (mark)
		PREFIX + "?", // list-keys
	),
);

// Command-prompt commands, reaching parser and layout code the default
// bindings don't. Each entry opens the prompt, types the command and runs it.
export const promptCommands = typeFromSet(
	CharSet.fromLiterals(
		...[
			"split-window",
			"split-window -h",
			"resize-pane -U 3",
			"resize-pane -D 3",
			"resize-pane -L 8",
			"resize-pane -R 8",
			"select-layout even-horizontal",
			"select-layout even-vertical",
			"select-layout main-horizontal",
			"select-layout main-vertical",
			"select-layout tiled",
			"rotate-window",
			"swap-window -t 0",
			"new-window",
			"respawn-pane -k",
			"clear-history",
			"display-message #{pane_id}",
			"setw window-status-current-style bg=red",
			"refresh-client",
		].map((command) => PREFIX + ":" + command + "\r"),
	),
);

// Bracketed paste of text with wide and combining characters, exercising the
// paste path (input.c) rather than key-by-key input.
export const pasteInput = typeFromSet(
	CharSet.fromLiterals(
		"\x1b[200~hello world\x1b[201~",
		"\x1b[200~日本語のテキスト\x1b[201~",
		"\x1b[200~ééé combining\x1b[201~",
		"\x1b[200~🙂👍🏽🇳🇴 emoji\x1b[201~",
		"\x1b[200~line one\rline two\x1b[201~",
	),
);

// Resizes within a reasonable range, exercising layout resize and reflow.
export const resizes = actions(() =>
	[
		{ columns: 40, rows: 10 },
		{ columns: 60, rows: 20 },
		{ columns: 80, rows: 24 },
		{ columns: 100, rows: 40 },
		{ columns: 120, rows: 48 },
		{ columns: 160, rows: 60 },
	].map((size) => ({ Resize: { size } })),
);

// Wheel scrolling; with the mouse enabled in tmux.conf this enters copy mode
// and scrolls through history.
export const scrolls = actions(() => [{ ScrollUp: {} }, { ScrollDown: {} }]);

// The overall input mix: mostly plain typing (echoed into the pane's shell
// and, in copy mode or prompts, fed to tmux itself) with regular bursts of
// tmux commands and terminal events.
export const inputs = weighted([
	[30, typeFromSet(CharSets.UNICODE_SAFE)],
	[15, typeFromSet(CharSets.CONTROL_ALL)],
	[20, prefixCommands],
	[8, promptCommands],
	[4, pasteInput],
	[3, resizes],
	[2, scrolls],
]);
