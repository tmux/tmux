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
#include <sys/utsname.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Show various information about server.
 */

enum cmd_retval	 cmd_server_info_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_server_info_entry = {
	"server-info", "info",
	"", 0, 0,
	"",
	0,
	NULL,
	NULL,
	cmd_server_info_exec
};

enum cmd_retval
cmd_server_info_exec(unused struct cmd *self, struct cmd_q *cmdq)
{
	struct tty_term				*term;
	struct client				*c;
	struct session				*s;
	struct winlink				*wl;
	struct window				*w;
	struct window_pane			*wp;
	struct tty_code				*code;
	const struct tty_term_code_entry	*ent;
	struct utsname				 un;
	struct job				*job;
	struct grid				*gd;
	struct grid_line			*gl;
	u_int		 			 i, j, k, lines;
	size_t					 size;
	char					 out[80];
	char					*tim;
	time_t		 			 t;

	tim = ctime(&start_time);
	*strchr(tim, '\n') = '\0';
	cmdq_print(cmdq,
	    "tmux " VERSION ", pid %ld, started %s", (long) getpid(), tim);
	cmdq_print(cmdq, "socket path %s, debug level %d", socket_path,
	    debug_level);
	if (uname(&un) >= 0) {
		cmdq_print(cmdq, "system is %s %s %s %s",
		    un.sysname, un.release, un.version, un.machine);
	}
	if (cfg_file != NULL)
		cmdq_print(cmdq, "configuration file is %s", cfg_file);
	else
		cmdq_print(cmdq, "configuration file not specified");
	cmdq_print(cmdq, "protocol version is %d", PROTOCOL_VERSION);
	cmdq_print(cmdq, "%s", "");

	cmdq_print(cmdq, "Clients:");
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		cmdq_print(cmdq,"%2d: %s (%d, %d): %s [%ux%u %s bs=%hho "
		    "class=%u] [flags=0x%x/0x%x, references=%u]", i,
		    c->tty.path, c->ibuf.fd, c->tty.fd, c->session->name,
		    c->tty.sx, c->tty.sy, c->tty.termname,
		    c->tty.tio.c_cc[VERASE], c->tty.class,
		    c->flags, c->tty.flags, c->references);
	}
	cmdq_print(cmdq, "%s", "");

	cmdq_print(cmdq, "Sessions: [%zu]", sizeof (struct grid_cell));
	RB_FOREACH(s, sessions, &sessions) {
		t = s->creation_time.tv_sec;
		tim = ctime(&t);
		*strchr(tim, '\n') = '\0';

		cmdq_print(cmdq, "%2u: %s: %u windows (created %s) [%ux%u] "
		    "[flags=0x%x]", s->id, s->name,
		    winlink_count(&s->windows), tim, s->sx, s->sy, s->flags);
		RB_FOREACH(wl, winlinks, &s->windows) {
			w = wl->window;
			cmdq_print(cmdq, "%4u: %s [%ux%u] [flags=0x%x, "
			    "references=%u, last layout=%d]", wl->idx, w->name,
			    w->sx, w->sy, w->flags, w->references,
			    w->lastlayout);
			j = 0;
			TAILQ_FOREACH(wp, &w->panes, entry) {
				lines = size = 0;
				gd = wp->base.grid;
				for (k = 0; k < gd->hsize + gd->sy; k++) {
					gl = &gd->linedata[k];
					if (gl->celldata == NULL)
						continue;
					lines++;
					size += gl->cellsize *
					    sizeof *gl->celldata;
				}
				cmdq_print(cmdq,
				    "%6u: %s %lu %d %u/%u, %zu bytes", j,
				    wp->tty, (u_long) wp->pid, wp->fd, lines,
				    gd->hsize + gd->sy, size);
				j++;
			}
		}
	}
	cmdq_print(cmdq, "%s", "");

	cmdq_print(cmdq, "Terminals:");
	LIST_FOREACH(term, &tty_terms, entry) {
		cmdq_print(cmdq, "%s [references=%u, flags=0x%x]:",
		    term->name, term->references, term->flags);
		for (i = 0; i < NTTYCODE; i++) {
			ent = &tty_term_codes[i];
			code = &term->codes[ent->code];
			switch (code->type) {
			case TTYCODE_NONE:
				cmdq_print(cmdq, "%2u: %s: [missing]",
				    ent->code, ent->name);
				break;
			case TTYCODE_STRING:
				strnvis(out, code->value.string, sizeof out,
				    VIS_OCTAL|VIS_TAB|VIS_NL);
				cmdq_print(cmdq, "%2u: %s: (string) %s",
				    ent->code, ent->name, out);
				break;
			case TTYCODE_NUMBER:
				cmdq_print(cmdq, "%2u: %s: (number) %d",
				    ent->code, ent->name, code->value.number);
				break;
			case TTYCODE_FLAG:
				cmdq_print(cmdq, "%2u: %s: (flag) %s",
				    ent->code, ent->name,
				    code->value.flag ? "true" : "false");
				break;
			}
		}
	}
	cmdq_print(cmdq, "%s", "");

	cmdq_print(cmdq, "Jobs:");
	LIST_FOREACH(job, &all_jobs, lentry) {
		cmdq_print(cmdq, "%s [fd=%d, pid=%d, status=%d]",
		    job->cmd, job->fd, job->pid, job->status);
	}

	return (CMD_RETURN_NORMAL);
}
