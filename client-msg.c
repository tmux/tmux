/* $Id: client-msg.c,v 1.23 2009-07-30 20:21:55 tcunha Exp $ */

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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

int	client_msg_fn_detach(struct hdr *, struct client_ctx *);
int	client_msg_fn_error(struct hdr *, struct client_ctx *);
int	client_msg_fn_shutdown(struct hdr *, struct client_ctx *);
int	client_msg_fn_exit(struct hdr *, struct client_ctx *);
int	client_msg_fn_exited(struct hdr *, struct client_ctx *);
int	client_msg_fn_suspend(struct hdr *, struct client_ctx *);

struct client_msg {
	enum msgtype   type;
	int	       (*fn)(struct hdr *, struct client_ctx *);
};
struct client_msg client_msg_table[] = {
	{ MSG_DETACH, client_msg_fn_detach },
	{ MSG_ERROR, client_msg_fn_error },
	{ MSG_EXIT, client_msg_fn_exit },
	{ MSG_EXITED, client_msg_fn_exited },
	{ MSG_SHUTDOWN, client_msg_fn_shutdown },
	{ MSG_SUSPEND, client_msg_fn_suspend },
};

int
client_msg_dispatch(struct client_ctx *cctx)
{
	struct hdr		 hdr;
	struct client_msg	*msg;
	u_int		 	 i;

	if (BUFFER_USED(cctx->srv_in) < sizeof hdr)
		return (1);
	memcpy(&hdr, BUFFER_OUT(cctx->srv_in), sizeof hdr);
	if (BUFFER_USED(cctx->srv_in) < (sizeof hdr) + hdr.size)
		return (1);
	buffer_remove(cctx->srv_in, sizeof hdr);

	for (i = 0; i < nitems(client_msg_table); i++) {
		msg = client_msg_table + i;
		if (msg->type == hdr.type)
			return (msg->fn(&hdr, cctx));
	}
	fatalx("unexpected message");
}

int
client_msg_fn_error(struct hdr *hdr, struct client_ctx *cctx)
{
	struct msg_print_data	data;

	if (hdr->size < sizeof data)
		fatalx("bad MSG_PRINT size");
	buffer_read(cctx->srv_in, &data, sizeof data);

	data.msg[(sizeof data.msg) - 1] = '\0';
	cctx->errstr = xstrdup(data.msg);

	return (-1);
}

int
client_msg_fn_detach(struct hdr *hdr, struct client_ctx *cctx)
{
	if (hdr->size != 0)
		fatalx("bad MSG_DETACH size");

	client_write_server(cctx, MSG_EXITING, NULL, 0);
	cctx->exittype = CCTX_DETACH;

	return (0);
}

int
client_msg_fn_shutdown(
    struct hdr *hdr, struct client_ctx *cctx)
{
	if (hdr->size != 0)
		fatalx("bad MSG_SHUTDOWN size");

	client_write_server(cctx, MSG_EXITING, NULL, 0);
	cctx->exittype = CCTX_SHUTDOWN;

	return (0);
}

int
client_msg_fn_exit(struct hdr *hdr, struct client_ctx *cctx)
{
	if (hdr->size != 0)
		fatalx("bad MSG_EXIT size");

	client_write_server(cctx, MSG_EXITING, NULL, 0);
	cctx->exittype = CCTX_EXIT;

	return (0);
}

int
client_msg_fn_exited(struct hdr *hdr, unused struct client_ctx *cctx)
{
	if (hdr->size != 0)
		fatalx("bad MSG_EXITED size");

	return (-1);
}

int
client_msg_fn_suspend(struct hdr *hdr, unused struct client_ctx *cctx)
{
	struct sigaction	 act;

	if (hdr->size != 0)
		fatalx("bad MSG_SUSPEND size");

	memset(&act, 0, sizeof act);
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	act.sa_handler = SIG_DFL;
	if (sigaction(SIGTSTP, &act, NULL) != 0)
		fatal("sigaction failed");

	act.sa_handler = sighandler;
	if (sigaction(SIGCONT, &act, NULL) != 0)
		fatal("sigaction failed");

	kill(getpid(), SIGTSTP);

	return (0);
}
