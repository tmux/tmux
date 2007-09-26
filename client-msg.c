/* $Id: client-msg.c,v 1.1 2007-09-26 13:43:14 nicm Exp $ */

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

int	client_msg_fn_output(struct hdr *, struct client_ctx *, const char **);
int	client_msg_fn_pause(struct hdr *, struct client_ctx *, const char **);
int	client_msg_fn_exit(struct hdr *, struct client_ctx *, const char **);

struct client_msg {
	enum hdrtype   type;
	
	int	       (*fn)(struct hdr *, struct client_ctx *, const char **);
};
struct client_msg client_msg_table[] = {
	{ MSG_OUTPUT, client_msg_fn_output },
	{ MSG_PAUSE, client_msg_fn_pause },
	{ MSG_EXIT, client_msg_fn_exit },
};
#define NCLIENTMSG (sizeof client_msg_table / sizeof client_msg_table[0])

int
client_msg_dispatch(struct client_ctx *cctx, const char **error)
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

/* Output message from client. */
int
client_msg_fn_output(
    struct hdr *hdr, struct client_ctx *cctx, unused const char **error)
{
	local_output(cctx->srv_in, hdr->size);
	return (0);
}

/* Pause message from server. */
int
client_msg_fn_pause(
    struct hdr *hdr, unused struct client_ctx *cctx, unused const char **error)
{
	if (hdr->size != 0)
		fatalx("bad MSG_PAUSE size");
	return (1);
}

/* Exit message from server. */
int
client_msg_fn_exit(
    struct hdr *hdr, unused struct client_ctx *cctx, unused const char **error)
{
	if (hdr->size != 0)
		fatalx("bad MSG_EXIT size");
	return (-1);
}
