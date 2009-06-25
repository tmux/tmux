/* $OpenBSD: cmd-lock-server.c,v 1.2 2009/06/25 06:00:45 nicm Exp $ */

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

#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Lock server.
 */

int	cmd_lock_server_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_lock_server_entry = {
	"lock-server", "lock",
	"",
	0,
	NULL,
	NULL,
	cmd_lock_server_exec,
	NULL,
	NULL,
	NULL,
	NULL,
};

int
cmd_lock_server_exec(unused struct cmd *self, unused struct cmd_ctx *ctx)
{
	server_lock();

	return (0);
}
