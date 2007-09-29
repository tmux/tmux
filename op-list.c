/* $Id: op-list.c,v 1.6 2007-09-29 15:06:00 nicm Exp $ */

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
op_list_sessions(char *path, int argc, unused char **argv)
{
	struct client_ctx	cctx;
	char		       *tim;
	struct sessions_data    data;
	struct sessions_entry	ent;
	struct pollfd		pfd;
	struct hdr		hdr;

	if (argc != 1)
		return (usage("list-sessions"));

	if (client_init(path, &cctx, 0) != 0)
		return (1);
	client_write_server(&cctx, MSG_SESSIONS, &data, sizeof data);

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

		if (hdr.type == MSG_ERROR) {
			if (hdr.size > INT_MAX - 1)
				fatalx("bad MSG_ERROR size");
			log_warnx(
			    "%.*s", (int) hdr.size, BUFFER_OUT(cctx.srv_in));
			return (1);
		}		
		if (hdr.type != MSG_SESSIONS)
			fatalx("unexpected message");

		if (hdr.size < sizeof data)
			fatalx("bad MSG_SESSIONS size");
		buffer_read(cctx.srv_in, &data, sizeof data);
		hdr.size -= sizeof data; 
		if (data.sessions == 0 && hdr.size == 0)
			return (0);
		if (hdr.size < data.sessions * sizeof ent)
			fatalx("bad MSG_SESSIONS size");

		while (data.sessions-- > 0) {
			buffer_read(cctx.srv_in, &ent, sizeof ent);

			tim = ctime(&ent.tim);
			*strchr(tim, '\n') = '\0';

			printf("%s: %u windows "
			    "(created %s)\n", ent.name, ent.windows, tim);
		}

		return (0);
	}
}

int
op_list_windows(char *path, int argc, char **argv)
{
	struct client_ctx	cctx;
	char			name[MAXNAMELEN];
	int			opt;
	struct windows_data	data;
	struct windows_entry	ent;
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
			return (usage("list-windows [-s session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("list-windows [-s session]"));

	if (client_init(path, &cctx, 0) != 0)
		return (1);
 
	client_fill_sessid(&data.sid, name);
	client_write_server(&cctx, MSG_WINDOWS, &data, sizeof data);

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
		
		if (hdr.type == MSG_ERROR) {
			if (hdr.size > INT_MAX - 1)
				fatalx("bad MSG_ERROR size");
			log_warnx(
			    "%.*s", (int) hdr.size, BUFFER_OUT(cctx.srv_in));
			return (1);
		}		
		if (hdr.type != MSG_WINDOWS)
			fatalx("unexpected message");

		if (hdr.size < sizeof data)
			fatalx("bad MSG_WINDOWS size");
		buffer_read(cctx.srv_in, &data, sizeof data);
		hdr.size -= sizeof data; 
		if (data.windows == 0 && hdr.size == 0) {
			log_warnx("session not found: %s", name);
			return (1);
		}
		if (hdr.size < data.windows * sizeof ent)
			fatalx("bad MSG_WINDOWS size");

		while (data.windows-- > 0) {
			buffer_read(cctx.srv_in, &ent, sizeof ent);

			if (*ent.title != '\0') {
				printf("%u: %s \"%s\" (%s)\n", ent.idx,
				    ent.name, ent.title, ent.tty);
			} else {
				printf("%u: %s (%s)\n",
				    ent.idx, ent.name, ent.tty);
			}
		}

		return (0);
	}
}
