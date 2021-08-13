/* $OpenBSD$ */

/*
 * Copyright (c) 2021 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#ifndef TMUX_PROTOCOL_H
#define TMUX_PROTOCOL_H

/* Protocol version. */
#define PROTOCOL_VERSION 8

/* Message types. */
enum msgtype {
	MSG_VERSION = 12,

	MSG_IDENTIFY_FLAGS = 100,
	MSG_IDENTIFY_TERM,
	MSG_IDENTIFY_TTYNAME,
	MSG_IDENTIFY_OLDCWD, /* unused */
	MSG_IDENTIFY_STDIN,
	MSG_IDENTIFY_ENVIRON,
	MSG_IDENTIFY_DONE,
	MSG_IDENTIFY_CLIENTPID,
	MSG_IDENTIFY_CWD,
	MSG_IDENTIFY_FEATURES,
	MSG_IDENTIFY_STDOUT,
	MSG_IDENTIFY_LONGFLAGS,
	MSG_IDENTIFY_TERMINFO,

	MSG_COMMAND = 200,
	MSG_DETACH,
	MSG_DETACHKILL,
	MSG_EXIT,
	MSG_EXITED,
	MSG_EXITING,
	MSG_LOCK,
	MSG_READY,
	MSG_RESIZE,
	MSG_SHELL,
	MSG_SHUTDOWN,
	MSG_OLDSTDERR, /* unused */
	MSG_OLDSTDIN, /* unused */
	MSG_OLDSTDOUT, /* unused */
	MSG_SUSPEND,
	MSG_UNLOCK,
	MSG_WAKEUP,
	MSG_EXEC,
	MSG_FLAGS,

	MSG_READ_OPEN = 300,
	MSG_READ,
	MSG_READ_DONE,
	MSG_WRITE_OPEN,
	MSG_WRITE,
	MSG_WRITE_READY,
	MSG_WRITE_CLOSE
};

/*
 * Message data.
 *
 * Don't forget to bump PROTOCOL_VERSION if any of these change!
 */
struct msg_command {
	int	argc;
}; /* followed by packed argv */

struct msg_read_open {
	int	stream;
	int	fd;
}; /* followed by path */

struct msg_read_data {
	int	stream;
};

struct msg_read_done {
	int	stream;
	int	error;
};

struct msg_write_open {
	int	stream;
	int	fd;
	int	flags;
}; /* followed by path */

struct msg_write_data {
	int	stream;
}; /* followed by data */

struct msg_write_ready {
	int	stream;
	int	error;
};

struct msg_write_close {
	int	stream;
};

#endif /* TMUX_PROTOCOL_H */
