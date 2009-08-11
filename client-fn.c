/* $OpenBSD$ */

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

void
client_fill_session(struct msg_command_data *data)
{
	char		*env, *ptr1, *ptr2, buf[256];
	size_t		 len;
	const char	*errstr;
	long long	 ll;

	data->pid = -1;
	if ((env = getenv("TMUX")) == NULL)
		return;

	if ((ptr2 = strrchr(env, ',')) == NULL || ptr2 == env)
		return;
	for (ptr1 = ptr2 - 1; ptr1 > env && *ptr1 != ','; ptr1--)
		;
	if (*ptr1 != ',')
		return;
	ptr1++;
	ptr2++;

	len = ptr2 - ptr1 - 1;
	if (len > (sizeof buf) - 1)
		return;
	memcpy(buf, ptr1, len);
	buf[len] = '\0';

	ll = strtonum(buf, 0, LONG_MAX, &errstr);
	if (errstr != NULL)
		return;
	data->pid = ll;

	ll = strtonum(ptr2, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return;
	data->idx = ll;
}

void
client_write_server(
    struct client_ctx *cctx, enum msgtype type, void *buf, size_t len)
{
	imsg_compose(&cctx->ibuf, type, PROTOCOL_VERSION, -1, -1, buf, len);
}

void
client_suspend(void)
{
	struct sigaction	 act;

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
}
