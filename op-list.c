/* $Id: op-list.c,v 1.1 2007-09-26 13:43:15 nicm Exp $ */

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

/* List sessions or windows. */
void
op_list(char *name)
{
	struct sessions_data	 sd;
	struct windows_data	 wd;
	struct pollfd	 	 pfd;
	struct hdr		 hdr;

	/* Send query data. */
	if (*name == '\0') {
		hdr.type = MSG_SESSIONS;
		hdr.size = sizeof sd;
		buffer_write(server_out, &hdr, sizeof hdr);
		buffer_write(server_out, &sd, hdr.size);
	} else {
		hdr.type = MSG_WINDOWS;
		hdr.size = sizeof wd;
		buffer_write(server_out, &hdr, sizeof hdr);
		strlcpy(wd.name, name, sizeof wd.name);
		buffer_write(server_out, &wd, hdr.size);
	}

	/* Main loop. */
	for (;;) {
		/* Set up pollfd. */
		pfd.fd = server_fd;
		pfd.events = POLLIN;
		if (BUFFER_USED(server_out) > 0)
			pfd.events |= POLLOUT;

		/* Do the poll. */
		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			err(1, "poll");
		}

		/* Read/write from sockets. */
		if (buffer_poll(&pfd, server_in, server_out) != 0)
			errx(1, "lost server"); 

		/* Process data. */
		process_list(name);
	}
}

void
op_list_process(const char *name)
{
	struct sessions_data	 sd;
	struct sessions_entry	 se;
	struct windows_data	 wd;
	struct windows_entry	 we;
	struct hdr		 hdr;
	char		        *tim;
	
	for (;;) {
		if (BUFFER_USED(server_in) < sizeof hdr)
			break;
		memcpy(&hdr, BUFFER_OUT(server_in), sizeof hdr);
		if (BUFFER_USED(server_in) < (sizeof hdr) + hdr.size)
			break;
		buffer_remove(server_in, sizeof hdr);
		
		switch (hdr.type) {
		case MSG_SESSIONS:
			if (hdr.size < sizeof sd)
				errx(1, "bad MSG_SESSIONS size");
			buffer_read(server_in, &sd, sizeof sd);
			hdr.size -= sizeof sd; 
			if (sd.sessions == 0 && hdr.size == 0)
				exit(0);
			if (hdr.size < sd.sessions * sizeof se)
				errx(1, "bad MSG_SESSIONS size");
			while (sd.sessions-- > 0) {
				buffer_read(server_in, &se, sizeof se);
				tim = ctime(&se.tim);
				*strchr(tim, '\n') = '\0';
				printf("%s: %u windows (created %s)\n",
				    se.name, se.windows, tim);
			}
			exit(0);
		case MSG_WINDOWS:
			if (hdr.size < sizeof wd)
				errx(1, "bad MSG_WINDOWS size");
			buffer_read(server_in, &wd, sizeof wd);
			hdr.size -= sizeof wd; 
			if (wd.windows == 0 && hdr.size == 0)
				errx(1, "session not found: %s", name);
			if (hdr.size < wd.windows * sizeof we)
				errx(1, "bad MSG_WINDOWS size");
			while (wd.windows-- > 0) {
				buffer_read(server_in, &we, sizeof we);
				if (*we.title != '\0') {
					printf("%u: %s \"%s\" (%s)\n",
					    we.idx, we.name, we.title, we.tty); 
				} else {
					printf("%u: %s (%s)\n",
					    we.idx, we.name, we.tty);
				}
			}
			exit(0);
		default:
			fatalx("unexpected message");
		}
	}
}
