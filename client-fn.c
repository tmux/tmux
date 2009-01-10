/* $Id: client-fn.c,v 1.5 2009-01-10 14:43:43 nicm Exp $ */

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
client_fill_session(struct msg_command_data *data)
{
	char		*env, *ptr, buf[256];
	const char	*errstr;
	long long	 ll;

	data->pid = -1;
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
	data->idx = ll;

	ll = strtonum(buf, 0, LONG_MAX, &errstr);
	if (errstr != NULL)
		return;
	data->pid = ll;
}

void
client_write_server(
    struct client_ctx *cctx, enum hdrtype type, void *buf, size_t len)
{
	struct hdr	hdr;

	hdr.type = type;
	hdr.size = len;
	buffer_write(cctx->srv_out, &hdr, sizeof hdr);

	if (buf != NULL && len > 0)
		buffer_write(cctx->srv_out, buf, len);
}

void
client_write_server2(struct client_ctx *cctx,
    enum hdrtype type, void *buf1, size_t len1, void *buf2, size_t len2)
{
	struct hdr	hdr;

	hdr.type = type;
	hdr.size = len1 + len2;
	buffer_write(cctx->srv_out, &hdr, sizeof hdr);

	if (buf1 != NULL && len1 > 0)
		buffer_write(cctx->srv_out, buf1, len1);
	if (buf2 != NULL && len2 > 0)
		buffer_write(cctx->srv_out, buf2, len2);
}
