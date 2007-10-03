/* $Id: op.c,v 1.12 2007-10-03 12:43:47 nicm Exp $ */

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
op_new_session(char *path, int argc, char **argv)
{
	struct new_data	 	data;	
	struct client_ctx	cctx;
	char			name[MAXNAMELEN];
	int			opt, detached;

	*name = '\0';
	detached = 0;
	optind = 1;
	while ((opt = getopt(argc, argv, "s:d?")) != EOF) {
		switch (opt) {
		case 's':
			if (strlcpy(name, optarg, sizeof name) >= sizeof name) {
				log_warnx("session name too long: %s", optarg);
				return (1);
			}
			break;
		case 'd':
			detached = 1;
			break;
		case '?':
		default:
			return (usage("new-session [-d] [-s session]"));
		}
	}	
	argc -= optind;
	argv += optind;			
	if (argc != 0)
		return (usage("new-session [-s session]"));

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	strlcpy(data.name, name, sizeof data.name);
	data.sx = cctx.ws.ws_col;
	data.sy = cctx.ws.ws_row;
	client_write_server(&cctx, MSG_NEW, &data, sizeof data);

	if (detached)
		return (client_flush(&cctx));
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
op_rename_window(char *path, int argc, char **argv)
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
			return (usage(
			    "rename-window [-s session] [-i index] name"));
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		return (usage("rename-window [-s session] [-i index] name"));

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

int
op_bind_key(char *path, int argc, char **argv)
{
	struct bind_data	data;	
	struct client_ctx	cctx;
	int			opt;
	const char	       *errstr;
	char		       *str;
	size_t			len;
 	const struct bind      *bind;
	
	optind = 1;
	while ((opt = getopt(argc, argv, "?")) != EOF) {
		switch (opt) {
		default:
			return (usage("bind-key key command [argument]"));
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2 && argc != 3)
		return (usage("bind-key key command [argument]"));

	if ((data.key = key_string_lookup(argv[0])) == KEYC_NONE) {
		log_warnx("unknown key: %s", argv[0]);
		return (1);
	}
	if (strlcpy(data.cmd, argv[1], sizeof data.cmd) >= sizeof data.cmd) {
		log_warnx("command too long: %s", argv[1]);
		return (1);
	}

	if ((bind = cmd_lookup_bind(data.cmd)) == NULL) {
		log_warnx("unknown command: %s", data.cmd);
		return (1);
	}

	str = NULL;
	len = 0;
	if (bind->flags & BIND_USER) {
		if (argc != 3) {
			log_warnx("%s requires an argument", data.cmd);
			return (1);
		}

		data.flags |= BIND_USER;
		if (bind->flags & BIND_STRING) {
			data.flags |= BIND_STRING;
			str = argv[2];
			len = strlen(str);
		} else if (bind->flags & BIND_NUMBER) {
			data.flags |= BIND_NUMBER;
			data.num = strtonum(argv[2], 0, UINT_MAX, &errstr);
			if (errstr != NULL) {
				log_warnx("argument %s: %s", errstr, argv[2]); 
				return (1);
			}
		} else
			fatalx("no argument type");
	} else {
		if (argc != 2) {
			log_warnx("%s cannot have an argument", data.cmd);
			return (1);
		}

		data.flags = 0;
	}

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	client_write_server2(&cctx, MSG_BINDKEY, &data, sizeof data, str, len);

	return (client_flush(&cctx));
}

int
op_unbind_key(char *path, int argc, char **argv)
{
	struct bind_data	data;	
	struct client_ctx	cctx;
	int			opt;

	optind = 1;
	while ((opt = getopt(argc, argv, "?")) != EOF) {
		switch (opt) {
		default:
			return (usage("unbind-key key"));
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		return (usage("unbind-key key"));

	if ((data.key = key_string_lookup(argv[0])) == KEYC_NONE) {
		log_warnx("unknown key: %s", argv[0]);
		return (1);
	}

	if (client_init(path, &cctx, 1) != 0)
		return (1);

	client_write_server(&cctx, MSG_UNBINDKEY, &data, sizeof data);

	return (client_flush(&cctx));
}
