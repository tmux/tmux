/* $Id: op.c,v 1.8 2007-09-29 13:22:15 nicm Exp $ */

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

int
op_new(char *path, int argc, char **argv)
{
	struct new_data	 	data;	
	struct client_ctx	cctx;
	char			name[MAXNAMELEN];
	int			opt;

	*name = '\0';
	optind = 1;
	while ((opt = getopt(argc, argv, "s:?")) != EOF) {
		switch (opt) {
		case 's':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name) {
				log_warnx("session name too long: %s", optarg);
				return (1);
			}
			break;
		case '?':
		default:
			return (usage("new [-s session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("new [-s session]"));

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	strlcpy(data.name, name, sizeof data.name);
	data.sx = cctx.ws.ws_col;
	data.sy = cctx.ws.ws_row;
	client_write_server(&cctx, MSG_NEW, &data, sizeof data);

	return (client_main(&cctx));
}

int
op_attach(char *path, int argc, char **argv)
{
	struct attach_data	data;
	struct client_ctx	cctx;
	char			name[MAXNAMELEN];
	int			opt;

	*name = '\0';
	optind = 1;
	while ((opt = getopt(argc, argv, "s:?")) != EOF) {
		switch (opt) {
		case 's':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name) {
				log_warnx("session name too long: %s", optarg);
				return (1);
			}
			break;
		case '?':
		default:
			return (usage("attach [-s session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("attach [-s session]"));

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	client_fill_sessid(&data.sid, name);
	data.sx = cctx.ws.ws_col;
	data.sy = cctx.ws.ws_row;
	client_write_server(&cctx, MSG_ATTACH, &data, sizeof data);

	return (client_main(&cctx));
}

int
op_rename(char *path, int argc, char **argv)
{
	struct rename_data	data;	
	struct client_ctx	cctx;
	char			sname[MAXNAMELEN];
	int			opt;  
	const char	       *errstr;

	*sname = '\0';
	data.idx = -1;
	optind = 1;
	while ((opt = getopt(argc, argv, "i:s:?")) != EOF) {
		switch (opt) {
		case 's':
			if (strlcpy(sname, optarg, sizeof sname) 
			    >= sizeof sname) {
				log_warnx("session name too long: %s", optarg);
				return (1);
			}
			break;
		case 'i':
			data.idx = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				log_warnx(
				    "window index %s: %s", errstr, optarg); 
				return (1);
			}
			break;
		case '?':
		default:
			return (usage("rename [-s session] [-i index] name"));
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		return (usage("rename [-s session] [-i index] name"));

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	client_fill_sessid(&data.sid, sname);
	if ((strlcpy(data.newname, argv[0], sizeof data.newname) 
	    >= sizeof data.newname)) {
		log_warnx("new window name too long: %s", argv[0]);
		return (1);
	}
	client_write_server(&cctx, MSG_RENAME, &data, sizeof data);

	return (client_flush(&cctx));
}
