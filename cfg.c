/* $Id$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Config file parser. Pretty quick and simple, each line is parsed into a
 * argv array and executed as a command.
 */

void printflike2 cfg_print(struct cmd_ctx *, const char *, ...);
void printflike2 cfg_error(struct cmd_ctx *, const char *, ...);

char			*cfg_cause;
int			 cfg_finished;
int	 		 cfg_references;
struct causelist	 cfg_causes;

/* ARGSUSED */
void printflike2
cfg_print(unused struct cmd_ctx *ctx, unused const char *fmt, ...)
{
}

/* ARGSUSED */
void printflike2
cfg_error(unused struct cmd_ctx *ctx, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	xvasprintf(&cfg_cause, fmt, ap);
	va_end(ap);
}

void printflike2
cfg_add_cause(struct causelist *causes, const char *fmt, ...)
{
	char	*cause;
	va_list	 ap;

	va_start(ap, fmt);
	xvasprintf(&cause, fmt, ap);
	va_end(ap);

	ARRAY_ADD(causes, cause);
}

/*
 * Load configuration file. Returns -1 for an error with a list of messages in
 * causes. Note that causes must be initialised by the caller!
 */
enum cmd_retval
load_cfg(const char *path, struct cmd_ctx *ctxin, struct causelist *causes)
{
	FILE		*f;
	u_int		 n;
	char		*buf, *copy, *line, *cause;
	size_t		 len, oldlen;
	struct cmd_list	*cmdlist;
	struct cmd_ctx	 ctx;
	enum cmd_retval	 retval;

	if ((f = fopen(path, "rb")) == NULL) {
		cfg_add_cause(causes, "%s: %s", path, strerror(errno));
		return (CMD_RETURN_ERROR);
	}

	cfg_references++;

	n = 0;
	line = NULL;
	retval = CMD_RETURN_NORMAL;
	while ((buf = fgetln(f, &len))) {
		/* Trim \n. */
		if (buf[len - 1] == '\n')
			len--;
		log_debug ("%s: %s", path, buf);

		/* Current line is the continuation of the previous one. */
		if (line != NULL) {
			oldlen = strlen(line);
			line = xrealloc(line, 1, oldlen + len + 1);
		} else {
			oldlen = 0;
			line = xmalloc(len + 1);
		}

		/* Append current line to the previous. */
		memcpy(line + oldlen, buf, len);
		line[oldlen + len] = '\0';
		n++;

		/* Continuation: get next line? */
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\\') {
			line[len - 1] = '\0';

			/* Ignore escaped backslash at EOL. */
			if (len > 1 && line[len - 2] != '\\')
				continue;
		}
		copy = line;
		line = NULL;

		/* Skip empty lines. */
		buf = copy;
		while (isspace((u_char)*buf))
			buf++;
		if (*buf == '\0')
			continue;

		if (cmd_string_parse(buf, &cmdlist, &cause) != 0) {
			free(copy);
			if (cause == NULL)
				continue;
			cfg_add_cause(causes, "%s: %u: %s", path, n, cause);
			free(cause);
			continue;
		}
		free(copy);
		if (cmdlist == NULL)
			continue;

		if (ctxin == NULL) {
			ctx.msgdata = NULL;
			ctx.curclient = NULL;
			ctx.cmdclient = NULL;
		} else {
			ctx.msgdata = ctxin->msgdata;
			ctx.curclient = ctxin->curclient;
			ctx.cmdclient = ctxin->cmdclient;
		}

		ctx.error = cfg_error;
		ctx.print = cfg_print;
		ctx.info = cfg_print;

		cfg_cause = NULL;
		switch (cmd_list_exec(cmdlist, &ctx)) {
		case CMD_RETURN_YIELD:
			if (retval != CMD_RETURN_ATTACH)
				retval = CMD_RETURN_YIELD;
			break;
		case CMD_RETURN_ATTACH:
			retval = CMD_RETURN_ATTACH;
			break;
		case CMD_RETURN_ERROR:
		case CMD_RETURN_NORMAL:
			break;
		}
		cmd_list_free(cmdlist);
		if (cfg_cause != NULL) {
			cfg_add_cause(causes, "%s: %d: %s", path, n, cfg_cause);
			free(cfg_cause);
		}
	}
	if (line != NULL) {
		cfg_add_cause(causes,
		    "%s: %d: line continuation at end of file", path, n);
		free(line);
	}
	fclose(f);

	cfg_references--;

	return (retval);
}

void
show_cfg_causes(struct session *s)
{
	struct window_pane	*wp;
	char			*cause;
	u_int			 i;

	if (s == NULL || ARRAY_EMPTY(&cfg_causes))
		return;

	wp = s->curw->window->active;

	window_pane_set_mode(wp, &window_copy_mode);
	window_copy_init_for_output(wp);
	for (i = 0; i < ARRAY_LENGTH(&cfg_causes); i++) {
		cause = ARRAY_ITEM(&cfg_causes, i);
		window_copy_add(wp, "%s", cause);
		free(cause);
	}
	ARRAY_FREE(&cfg_causes);
}
