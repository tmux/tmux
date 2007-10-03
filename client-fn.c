/* $Id: client-fn.c,v 1.1 2007-10-03 10:18:31 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

void
client_fill_sessid(struct sessid *sid, char name[MAXNAMELEN])
{
	char		*env, *ptr, buf[256];
	const char	*errstr;
	long long	 ll;

	strlcpy(sid->name, name, sizeof sid->name);

	sid->pid = -1;
	if ((env = getenv("TMUX")) == NULL)
		return;
	if ((ptr = strchr(env, ',')) == NULL)
		return;
	if ((size_t) (ptr - env) > sizeof buf)
		return;
	memcpy(buf, env, ptr - env);
	buf[ptr - env] = '\0';

	ll = strtonum(ptr + 1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return;
	sid->idx = ll;

	ll = strtonum(buf, 0, LLONG_MAX, &errstr);
	if (errstr != NULL)
		return;
	sid->pid = ll;
}

void
client_write_server(
    struct client_ctx *cctx, enum hdrtype type, void *buf, size_t len)
{
	struct hdr	hdr;

	hdr.type = type;
	hdr.size = len;
	buffer_write(cctx->srv_out, &hdr, sizeof hdr);
	if (len > 0)
		buffer_write(cctx->srv_out, buf, len);
}
