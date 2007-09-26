/* $Id: op.c,v 1.2 2007-09-26 14:08:16 nicm Exp $ */

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

#include <string.h>
#include <unistd.h>

#include "tmux.h"

int
op_new(char *path, int argc, unused char **argv)
{
	struct new_data	 	data;	
	struct client_ctx	cctx;
	char			name[MAXNAMELEN];
	int			opt;

	optind = 1;
	while ((opt = getopt(argc, argv, "n:?")) != EOF) {
		switch (opt) {
		case 'n':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name) {
				log_warnx("%s: session name too long", optarg);
				return (1);
			}
			break;
		case '?':
		default:
			return (usage("new [-n session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("new [-n session]"));

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	strlcpy(data.name, name, sizeof data.name);
	data.sx = cctx.ws.ws_col;
	data.sy = cctx.ws.ws_row;
	client_write_server(&cctx, MSG_NEW, &data, sizeof data);

	return (client_main(&cctx));
}

int
op_attach(char *path, int argc, unused char **argv)
{
	struct attach_data	data;
	struct client_ctx	cctx;
	char			name[MAXNAMELEN];
	int			opt;

	optind = 1;
	while ((opt = getopt(argc, argv, "n:?")) != EOF) {
		switch (opt) {
		case 'n':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name) {
				log_warnx("%s: session name too long", optarg);
				return (1);
			}
			break;
		case '?':
		default:
			return (usage("attach [-n session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("attach [-n session]"));

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	strlcpy(data.name, name, sizeof data.name);
	data.sx = cctx.ws.ws_col;
	data.sy = cctx.ws.ws_row;
	client_write_server(&cctx, MSG_ATTACH, &data, sizeof data);

	return (client_main(&cctx));
}

