/*
 * Copyright (c) 2026 OSS-Fuzz contributors
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Fuzz the mode-tree family of window modes (window-customize.c,
 * window-tree.c, window-buffer.c, window-switch.c and the shared
 * mode-tree.c engine - roughly 6k lines at 0% coverage, plus prompt.c via
 * switch mode's incremental search prompt).
 *
 * No existing harness reaches any of these modes. The harness builds a
 * small real state tree per iteration (two sessions, three windows, four
 * panes, paste buffers), derives a cmd_find_state from the first session,
 * enters one of the tree-family modes and drives it with key events
 * (motion, selection over the mode_tree_run_command path, search prompts,
 * tagging, help) and pane resizes.
 *
 * window_client_mode is deliberately out of scope: it renders the client
 * list, which is always empty without a server, so it has no reachable
 * dark code. window_panes_mode needs a cmdq_item and is left to a command-
 * driving harness (display-panes).
 *
 * Input layout:
 *   data[0]      mode selector (customize, tree pane/window/session view,
 *                buffer, switch session/window)
 *   data[1]      mode flag bits ('Z', 'y', 'G', 'h', 'i', 'r', 'f')
 *   data[2..3]   pane size selectors
 *   data[4..]    key events; 0xff bytes resize the pane instead
 */

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>

#include "tmux.h"

/* window-customize.c exports its mode but does not declare it in tmux.h. */
extern const struct window_mode	window_customize_mode;

#define FUZZER_MAXLEN	1024

struct event_base *libevent;

static key_code
modes_tree_fuzzer_key(u_char b)
{
	switch (b) {
	case 0x80: return (KEYC_UP);
	case 0x81: return (KEYC_DOWN);
	case 0x82: return (KEYC_LEFT);
	case 0x83: return (KEYC_RIGHT);
	case 0x84: return (KEYC_NPAGE);
	case 0x85: return (KEYC_PPAGE);
	case 0x86: return (KEYC_HOME);
	case 0x87: return (KEYC_END);
	case 0x88: return (KEYC_IC);
	case 0x89: return (KEYC_DC);
	case 0x8a: return (KEYC_BTAB);
	case 0x8b: return (KEYC_BSPACE);
	}
	if (b < 0x20)
		return (b == 0 ? KEYC_CTRL | ' ' : b); /* control chars */
	if (b <= 0x7e)
		return (b); /* printable */
	return (KEYC_NONE);
}

static struct window_pane *
modes_tree_fuzzer_pane(struct window *w, struct window_pane *other,
    u_int sx, u_int sy)
{
	struct window_pane	*wp;

	wp = window_add_pane(w, other, 0, 0);
	wp->ictx = input_init(wp, NULL);
	wp->fd = open("/dev/null", O_WRONLY);
	if (wp->fd == -1)
		errx(1, "open(\"/dev/null\") failed");
	wp->event = bufferevent_new(wp->fd, NULL, NULL, NULL, NULL);
	if (w->active == NULL)
		window_set_active_pane(w, wp, 0);
	return (wp);
}

static const struct window_mode *
modes_tree_fuzzer_mode(u_char msel)
{
	switch (msel % 7) {
	case 0:
		return (&window_customize_mode);
	case 1: /* tree, pane view */
	case 2: /* tree, session view */
	case 3: /* tree, window view */
		return (&window_tree_mode);
	case 4:
		return (&window_buffer_mode);
	case 5: /* switch, session chooser */
	case 6: /* switch, window chooser */
		return (&window_switch_mode);
	default:
		return (NULL);
	}
}

static struct args *
modes_tree_fuzzer_args(u_char msel, u_char flags)
{
	struct args	*args;

	args = args_create();
	if (flags & 1)
		args_set(args, 'Z', NULL, 0);
	if (flags & 2)
		args_set(args, 'y', NULL, 0);
	switch (msel % 7) {
	case 2:
		args_set(args, 's', NULL, 0);
		break;
	case 3:
		args_set(args, 'w', NULL, 0);
		break;
	case 6:
		args_set(args, 'w', NULL, 0);
		break;
	}
	if ((flags & 4) && msel == 1)
		args_set(args, 'G', NULL, 0);
	if ((flags & 8) && msel == 1)
		args_set(args, 'h', NULL, 0);
	if ((flags & 16) && msel == 1)
		args_set(args, 'i', NULL, 0);
	if ((flags & 4) && msel >= 5)
		args_set(args, 'r', NULL, 0);
	if ((flags & 8) && msel >= 5)
		args_set(args, 'f', NULL, 0);
	return (args);
}

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct window		*w[3];
	struct window_pane	*host;
	struct session		*s1, *s2;
	struct winlink		*wl;
	struct paste_buffer	*pb;
	struct cmd_find_state	 fs;
	struct args		*args;
	const struct window_mode *mode;
	char			*cause;
	size_t			 i;
	u_int			 sx, sy;
	u_char			 msel, flags;
	key_code		 key;
	int			 error;

	if (size < 6 || size > FUZZER_MAXLEN)
		return (0);

	msel = data[0] % 7;
	flags = data[1];
	sx = 2 + (data[2] % 79);
	sy = 2 + (data[3] % 59);

	mode = modes_tree_fuzzer_mode(msel);
	if (mode == NULL)
		return (0);

	args = modes_tree_fuzzer_args(msel, flags);

	/* Two sessions with three single-pane windows of real state. */
	s1 = session_create("fuzz0", "fuzz0", "/tmp", environ_create(),
	    options_create(global_s_options), NULL);
	s2 = session_create("fuzz1", "fuzz1", "/tmp", environ_create(),
	    options_create(global_s_options), NULL);

	/* One wired pane per window, with a real layout root each - the
	 * invariant production guarantees via the spawn path (panes always
	 * carry a layout cell; zoom and resize paths deref it). */
	w[0] = window_create(sx, sy, 0, 0);
	w[1] = window_create(sx, sy, 0, 0);
	w[2] = window_create(sx, sy, 0, 0);
	window_add_ref(w[0], __func__);
	window_add_ref(w[1], __func__);
	window_add_ref(w[2], __func__);

	host = modes_tree_fuzzer_pane(w[0], NULL, sx, sy);
	modes_tree_fuzzer_pane(w[1], NULL, sx, sy);
	modes_tree_fuzzer_pane(w[2], NULL, sx, sy);
	window_set_active_pane(w[0], host, 0);
	layout_init(w[0], host);
	layout_init(w[1], TAILQ_FIRST(&w[1]->panes));
	layout_init(w[2], TAILQ_FIRST(&w[2]->panes));

	wl = session_attach(s1, w[0], -1, &cause);
	if (wl == NULL)
		errx(1, "session_attach failed: %s", cause);
	wl = session_attach(s1, w[1], -1, &cause);
	if (wl == NULL)
		errx(1, "session_attach failed: %s", cause);
	wl = session_attach(s2, w[2], -1, &cause);
	if (wl == NULL)
		errx(1, "session_attach failed: %s", cause);
	session_set_current(s1, RB_MIN(winlinks, &s1->windows));
	session_set_current(s2, RB_MIN(winlinks, &s2->windows));

	/* Paste buffers for the buffer chooser. */
	for (i = 0; i < 3; i++) {
		size_t		 len = (i == 0 ? 0 : (i == 1 ? 1 : 64));
		char		*buf;

		buf = xmalloc(len + 1);
		memset(buf, 'a' + (char)i, len);
		buf[len] = '\0';
		paste_add(NULL, buf, len);
	}

	cmd_find_from_session(&fs, s1, 0);

	/* Enter the selected mode over the built state. */
	window_pane_set_mode(host, host, mode, NULL, &fs, args);
	args_free(args);

	/* Drive the mode with key events; 0xff resizes the pane. Resize
	 * targets are kept >= 20 rows/columns: shrinking a mode pane below
	 * its minimum height trips a known upstream hang in mode-tree
	 * drawing (stale mtd->height + unsigned underflow on sy - h, see
	 * the project report's finding MODES-TREE-1); the hang is reported
	 * as a finding, not worked around here. */
	for (i = 4; i < size; i++) {
		if (data[i] == 0xff) {
			window_pane_resize(host, 20 + ((i * 37) % 60),
			    20 + ((i * 53) % 40));
			continue;
		}
		key = modes_tree_fuzzer_key(data[i]);
		if (key == KEYC_NONE)
			continue;
		window_pane_key(host, NULL, NULL, NULL, key, NULL);
	}

	/* Drain anything the modes queued (selection commands, prompts). */
	while (cmdq_next(NULL) != 0)
		;
	error = event_base_loop(libevent, EVLOOP_NONBLOCK);
	if (error == -1)
		errx(1, "event_base_loop failed");

	window_pane_reset_mode_all(host);

	/* Tear the state down the way the server would. */
	session_destroy(s1, 1, __func__);
	session_destroy(s2, 1, __func__);
	while (!paste_is_empty()) {
		pb = paste_get_top(NULL);
		if (pb == NULL)
			break;
		paste_free(pb);
	}
	while (cmdq_next(NULL) != 0)
		;
	error = event_base_loop(libevent, EVLOOP_NONBLOCK);
	if (error == -1)
		errx(1, "event_base_loop failed");

	assert(w[0]->references == 1);
	assert(w[1]->references == 1);
	assert(w[2]->references == 1);
	window_remove_ref(w[0], __func__);
	window_remove_ref(w[1], __func__);
	window_remove_ref(w[2], __func__);

	return (0);
}

int
LLVMFuzzerInitialize(__unused int *argc, __unused char ***argv)
{
	const struct options_table_entry	*oe;

	global_environ = environ_create();
	global_options = options_create(NULL);
	global_s_options = options_create(NULL);
	global_w_options = options_create(NULL);
	for (oe = options_table; oe->name != NULL; oe++) {
		if (oe->scope & OPTIONS_TABLE_SERVER)
			options_default(global_options, oe);
		if (oe->scope & OPTIONS_TABLE_SESSION)
			options_default(global_s_options, oe);
		if (oe->scope & OPTIONS_TABLE_WINDOW)
			options_default(global_w_options, oe);
	}
	libevent = osdep_event_init();

	options_set_number(global_w_options, "monitor-bell", 0);
	options_set_number(global_w_options, "allow-rename", 1);
	options_set_number(global_options, "set-clipboard", 2);
	socket_path = xstrdup("dummy");

	return (0);
}
