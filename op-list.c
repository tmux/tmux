/* $Id: op-list.c,v 1.4 2007-09-27 09:52:03 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <errno.h>
#include <getopt.h>
#include <string.h>

#include "tmux.h"

int
op_list(char *path, int argc, char **argv)
{
	struct client_ctx	cctx;
	char			name[MAXNAMELEN], *tim;
	int			opt;
	struct sessions_data	sdata;
	struct sessions_entry	sent;
	struct windows_data	wdata;
	struct windows_entry	went;
	struct pollfd		pfd;
	struct hdr		hdr;

	*name = '\0';
	optind = 1;
	while ((opt = getopt(argc, argv, "s:?")) != EOF) {
		switch (opt) {
		case 's':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name) {
				log_warnx("%s: session name too long", optarg);
				return (1);
			}
			break;
		case '?':
		default:
			return (usage("list [-s session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("list [-s session]"));

	if (client_init(path, &cctx, 0) != 0)
		return (1);
 
	if (*name == '\0')
		client_write_server(&cctx, MSG_SESSIONS, &sdata, sizeof sdata);
	else {
		client_fill_sessid(&wdata.sid, name);
		client_write_server(&cctx, MSG_WINDOWS, &wdata, sizeof wdata);
	}

	for (;;) {
		pfd.fd = cctx.srv_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(cctx.srv_out) > 0)
			pfd.events |= POLLOUT;

		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			log_warn("poll");
			return (-1);
		}

		if (buffer_poll(&pfd, cctx.srv_in, cctx.srv_out) != 0) {
			log_warnx("lost server");
			return (-1);
		}

		if (BUFFER_USED(cctx.srv_in) < sizeof hdr)
			continue;
		memcpy(&hdr, BUFFER_OUT(cctx.srv_in), sizeof hdr);
		if (BUFFER_USED(cctx.srv_in) < (sizeof hdr) + hdr.size)
			continue;
		buffer_remove(cctx.srv_in, sizeof hdr);
		
		switch (hdr.type) {
		case MSG_SESSIONS:
			if (hdr.size < sizeof sdata)
				fatalx("bad MSG_SESSIONS size");
			buffer_read(cctx.srv_in, &sdata, sizeof sdata);
			hdr.size -= sizeof sdata; 
			if (sdata.sessions == 0 && hdr.size == 0)
				return (0);
			if (hdr.size < sdata.sessions * sizeof sent)
				fatalx("bad MSG_SESSIONS size");
			while (sdata.sessions-- > 0) {
				buffer_read(cctx.srv_in, &sent, sizeof sent);
				tim = ctime(&sent.tim);
				*strchr(tim, '\n') = '\0';
				printf("%s: %u windows (created %s)\n",
				    sent.name, sent.windows, tim);
			}
			return (0);
		case MSG_WINDOWS:
			if (hdr.size < sizeof wdata)
				fatalx("bad MSG_WINDOWS size");
			buffer_read(cctx.srv_in, &wdata, sizeof wdata);
			hdr.size -= sizeof wdata; 
			if (wdata.windows == 0 && hdr.size == 0) {
				log_warnx("session not found: %s", name);
				return (1);
			}
			if (hdr.size < wdata.windows * sizeof went)
				fatalx("bad MSG_WINDOWS size");
			while (wdata.windows-- > 0) {
				buffer_read(cctx.srv_in, &went, sizeof went);
				if (*went.title != '\0') {
					printf("%u: %s \"%s\" (%s)\n", went.idx,
					    went.name, went.title, went.tty);
				} else {
					printf("%u: %s (%s)\n",
					    went.idx, went.name, went.tty);
				}
			}
			return (0);
		default:
			fatalx("unexpected message");
		}
	}
}
