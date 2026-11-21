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
 * Fuzz tmux command execution against a real session state
 * (cmd-find.c, cmd-queue.c, session.c, layout-set.c, server-fn.c and the
 * cmd-* family - several thousand lines at or near 0% coverage; the
 * Introspector's top zero-coverage function cmd_new_session_exec lives
 * here).
 *
 * Each input is one tmux command line (multiple statements may be chained
 * with ';'). The harness builds a small real state tree (two sessions,
 * three windows with layout roots, wired panes, a paste buffer), derives a
 * cmd_find_state from the first session and executes the parsed command
 * list through the cmdq machinery exactly the way mode_tree_run_command
 * does - the same NULL-client context tmux config files run in, which
 * guarantees every command handles a NULL client.
 *
 * Threat model bound: commands that execute attacker text as trusted shell
 * commands or unbounded file I/O are screened out before execution - by
 * the first word of each ';' separated statement, including aliases
 * (run/run-shell, if/if-shell, pipe-pane, source/source-file,
 * load-buffer/loadb, save-buffer/saveb, lock/lock-server/lock-client/
 * lock-command), and by the #{sh: / #( format escapes anywhere (format
 * expansion runs those as shell commands). Those would blur the
 * trusted/attacker boundary rather than test it. Spawn-type commands
 * (new-session/new-window/split-window/respawn) exec default-shell,
 * pinned here to /bin/false so panes exit immediately; children are
 * reaped with a waitpid drain each iteration.
 *
 * Input layout:
 *   the whole input is one NUL-terminated command line, max 2048 bytes.
 */

#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/wait.h>

#include "tmux.h"

#define FUZZER_MAXLEN	2048

struct event_base *libevent;

/* Commands (first word of a statement, incl. aliases) screened out. */
static const char	*session_cmds_fuzzer_exec_commands[] = {
	"run",
	"run-shell",
	"if",
	"if-shell",
	"pipe-pane",
	"source",
	"source-file",
	"load-buffer",
	"loadb",
	"save-buffer",
	"saveb",
	"lock",
	"lock-server",
	"lock-client",
	"lock-command",
	NULL
};

/* Format escapes that run commands; screened anywhere in the input. */
static const char	*session_cmds_fuzzer_exec_formats[] = {
	"#{sh:",
	"#(",
	NULL
};

static int
session_cmds_fuzzer_word_excluded(const char *word, size_t len)
{
	u_int	i;

	for (i = 0; session_cmds_fuzzer_exec_commands[i] != NULL; i++) {
		if (strlen(session_cmds_fuzzer_exec_commands[i]) == len &&
		    strncmp(word, session_cmds_fuzzer_exec_commands[i],
		    len) == 0)
			return (1);
	}
	return (0);
}

static int
session_cmds_fuzzer_excluded_input(const char *cmd)
{
	char	copy[FUZZER_MAXLEN + 1];
	char	*p, *end;
	u_int	i;

	for (i = 0; session_cmds_fuzzer_exec_formats[i] != NULL; i++) {
		if (strstr(cmd, session_cmds_fuzzer_exec_formats[i]) != NULL)
			return (1);
	}

	strlcpy(copy, cmd, sizeof copy);
	p = copy;
	while (*p != '\0') {
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\0')
			break;
		end = p;
		while (*end != '\0' && *end != ' ' && *end != '\t' &&
		    *end != ';')
			end++;
		if (session_cmds_fuzzer_word_excluded(p, end - p))
			return (1);
		p = (*end == '\0' ? end : end + 1);
	}
	return (0);
}

static struct window_pane *
session_cmds_fuzzer_pane(struct window *w)
{
	struct window_pane	*wp;

	wp = window_add_pane(w, NULL, 0, 0);
	wp->ictx = input_init(wp, NULL);
	wp->fd = open("/dev/null", O_WRONLY);
	if (wp->fd == -1)
		errx(1, "open(\"/dev/null\") failed");
	wp->event = bufferevent_new(wp->fd, NULL, NULL, NULL, NULL);
	if (w->active == NULL)
		window_set_active_pane(w, wp, 0);
	return (wp);
}

int
LLVMFuzzerTestOneInput(const u_char *data, size_t size)
{
	struct session		*s1, *s2, *s, *snext;
	struct window		*w[3];
	struct window_pane	*host, *wp1, *wp2;
	struct winlink		*wl;
	struct paste_buffer	*pb;
	struct cmd_find_state	 fs;
	struct cmdq_state	*state;
	struct cmd_parse_input	 pi;
	char			*cmd, *cause = NULL;

	if (size == 0 || size > FUZZER_MAXLEN)
		return (0);

	cmd = xmalloc(size + 1);
	memcpy(cmd, data, size);
	cmd[size] = '\0';
	if (session_cmds_fuzzer_excluded_input(cmd)) {
		free(cmd);
		return (0);
	}

	/* Two sessions with three single-pane windows of real state. */
	s1 = session_create("base0", "base0", "/tmp", environ_create(),
	    options_create(global_s_options), NULL);
	s2 = session_create("base1", "base1", "/tmp", environ_create(),
	    options_create(global_s_options), NULL);
	if (s1 == NULL || s2 == NULL)
		errx(1, "session_create failed");

	w[0] = window_create(80, 24, 0, 0);
	w[1] = window_create(80, 24, 0, 0);
	w[2] = window_create(80, 24, 0, 0);
	window_add_ref(w[0], __func__);
	window_add_ref(w[1], __func__);
	window_add_ref(w[2], __func__);

	host = session_cmds_fuzzer_pane(w[0]);
	wp1 = session_cmds_fuzzer_pane(w[1]);
	wp2 = session_cmds_fuzzer_pane(w[2]);
	layout_init(w[0], host);
	layout_init(w[1], wp1);
	layout_init(w[2], wp2);

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

	/* A paste buffer for set/paste/show-buffer commands. */
	paste_add(NULL, xstrdup("payload"), 7);

	cmd_find_from_session(&fs, s1, 0);

	/* Execute the input as one command line (config-file context). */
	memset(&pi, 0, sizeof pi);
	pi.flags = CMD_PARSE_QUIET;
	state = cmdq_new_state(&fs, NULL, 0);
	cmd_parse_and_append(cmd, &pi, NULL, state, &cause);
	if (cause != NULL)
		free(cause);
	cmdq_free_state(state);

	/* Run the queue and pump the event loop. */
	while (cmdq_next(NULL) != 0)
		;
	event_base_loop(libevent, EVLOOP_NONBLOCK);
	while (cmdq_next(NULL) != 0)
		;
	event_base_loop(libevent, EVLOOP_NONBLOCK);

	/* Tear the state down the way the server would: every session
	 * (including ones the commands created), every paste buffer, every
	 * pane mode, and all spawned children. */
	window_pane_reset_mode_all(host);
	window_pane_reset_mode_all(wp1);
	window_pane_reset_mode_all(wp2);

	s = RB_MIN(sessions, &sessions);
	while (s != NULL) {
		snext = RB_NEXT(sessions, &sessions, s);
		session_destroy(s, 1, __func__);
		s = snext;
	}
	while (!paste_is_empty()) {
		pb = paste_get_top(NULL);
		if (pb == NULL)
			break;
		paste_free(pb);
	}
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
	while (cmdq_next(NULL) != 0)
		;
	event_base_loop(libevent, EVLOOP_NONBLOCK);
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	/*
	 * Release our own references last: commands may have killed these
	 * windows themselves, and with our reference still held the
	 * window structs remain valid until this point either way.
	 */
	window_remove_ref(w[0], __func__);
	window_remove_ref(w[1], __func__);
	window_remove_ref(w[2], __func__);

	free(cmd);
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

	/* Spawned panes must exit immediately (see the header comment). */
	options_set_string(global_s_options, "default-shell", 0, "%s",
	    "/bin/false");
	/* Production invariant: a server always has a proc (server.c does
	 * server_proc = proc_start("server")). Jobs and timers deref it. */
	server_proc = proc_start("session-cmds-fuzzer");
	options_set_number(global_w_options, "monitor-bell", 0);
	options_set_number(global_w_options, "allow-rename", 1);
	options_set_number(global_options, "set-clipboard", 2);
	socket_path = xstrdup("dummy");

	return (0);
}
