/* $Id: client-msg.c,v 1.6 2007-10-03 21:31:07 nicm Exp $ */

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
#include <unistd.h>

#include "tmux.h"

int	client_msg_fn_data(struct hdr *, struct client_ctx *, char **);
int	client_msg_fn_detach(struct hdr *, struct client_ctx *, char **);
int	client_msg_fn_error(struct hdr *, struct client_ctx *, char **);
int	client_msg_fn_okay(struct hdr *, struct client_ctx *, char **);
int	client_msg_fn_pause(struct hdr *, struct client_ctx *, char **);

struct client_msg {
	enum hdrtype   type;
	
	int	       (*fn)(struct hdr *, struct client_ctx *, char **);
};
struct client_msg client_msg_table[] = {
	{ MSG_DATA, client_msg_fn_data },
	{ MSG_DETACH, client_msg_fn_detach },
	{ MSG_ERROR, client_msg_fn_error },
	{ MSG_PAUSE, client_msg_fn_pause },
};
#define NCLIENTMSG (sizeof client_msg_table / sizeof client_msg_table[0])

int
client_msg_dispatch(struct client_ctx *cctx, char **error)
{
	struct hdr		 hdr;
	struct client_msg	*msg;
	u_int		 	 i;
	int			 n;

	for (;;) {
		if (BUFFER_USED(cctx->srv_in) < sizeof hdr)
			return (0);
		memcpy(&hdr, BUFFER_OUT(cctx->srv_in), sizeof hdr);
		if (BUFFER_USED(cctx->srv_in) < (sizeof hdr) + hdr.size)
			return (0);
		buffer_remove(cctx->srv_in, sizeof hdr);
		
		for (i = 0; i < NCLIENTMSG; i++) {
			msg = client_msg_table + i;
			if (msg->type == hdr.type) {
				if ((n = msg->fn(&hdr, cctx, error)) != 0)
					return (n);
				break;
			}
		}
		if (i == NCLIENTMSG)
			fatalx("unexpected message");
	}
}

int
client_msg_fn_data(
    struct hdr *hdr, struct client_ctx *cctx, unused char **error)
{
	local_output(cctx->srv_in, hdr->size);
	return (0);
}

int
client_msg_fn_pause(
    struct hdr *hdr, unused struct client_ctx *cctx, unused char **error)
{
	if (hdr->size != 0)
		fatalx("bad MSG_PAUSE size");
	return (1);
}

int
client_msg_fn_error(struct hdr *hdr, struct client_ctx *cctx, char **error)
{
	if (hdr->size > SIZE_MAX - 1)
		fatalx("bad MSG_ERROR size");

	*error = xmalloc(hdr->size + 1);
	buffer_read(cctx->srv_in, *error, hdr->size);
	(*error)[hdr->size] = '\0';

	return (-1);
}

int
client_msg_fn_detach(
    struct hdr *hdr, unused struct client_ctx *cctx, char **error)
{
	if (hdr->size != 0)
		fatalx("bad MSG_DETACH size");

	*error = NULL;

	return (-1);
}
