/* $Id: cmd-generic.c,v 1.36 2009-11-14 17:57:41 tcunha Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

int	cmd_getopt(int, char **, const char *, const char *);
int	cmd_parse_flags(int, const char *, uint64_t *);
size_t	cmd_print_flags(char *, size_t, size_t, uint64_t);
int	cmd_fill_argument(int, char **, char **, int, char **);

size_t
cmd_prarg(char *buf, size_t len, const char *prefix, char *arg)
{
	if (strchr(arg, ' ') != NULL)
		return (xsnprintf(buf, len, "%s\"%s\"", prefix, arg));
	return (xsnprintf(buf, len, "%s%s", prefix, arg));
}

/* Append two flag strings together and call getopt. */
int
cmd_getopt(int argc, char **argv, const char *flagstr, const char *chflagstr)
{
	char	tmp[BUFSIZ];

	if (strlcpy(tmp, flagstr, sizeof tmp) >= sizeof tmp)
		fatalx("strlcpy overflow");
	if (strlcat(tmp, chflagstr, sizeof tmp) >= sizeof tmp)
		fatalx("strlcat overflow");
	return (getopt(argc, argv, tmp));
}

/* Return if flag character is set. */
int
cmd_check_flag(uint64_t chflags, int flag)
{
	if (flag >= 'A' && flag <= 'Z')
		flag = 26 + flag - 'A';
	else if (flag >= 'a' && flag <= 'z')
		flag = flag - 'a';
	else
		return (0);
	return ((chflags & (1ULL << flag)) != 0);
}

/* Set flag character. */
void
cmd_set_flag(uint64_t *chflags, int flag)
{
	if (flag >= 'A' && flag <= 'Z')
		flag = 26 + flag - 'A';
	else if (flag >= 'a' && flag <= 'z')
		flag = flag - 'a';
	else
		return;
	(*chflags) |= (1ULL << flag);
}

/* If this option is expected, set it in chflags, otherwise return -1. */
int
cmd_parse_flags(int opt, const char *chflagstr, uint64_t *chflags)
{
	if (strchr(chflagstr, opt) == NULL)
		return (-1);
	cmd_set_flag(chflags, opt);
	return (0);
}

/* Print the flags present in chflags. */
size_t
cmd_print_flags(char *buf, size_t len, size_t off, uint64_t chflags)
{
	u_char	ch;
	size_t	boff = off;

	if (chflags == 0)
		return (0);
	off += xsnprintf(buf + off, len - off, " -");

	for (ch = 0; ch < 26; ch++) {
		if (cmd_check_flag(chflags, 'a' + ch))
			off += xsnprintf(buf + off, len - off, "%c", 'a' + ch);
		if (cmd_check_flag(chflags, 'A' + ch))
			off += xsnprintf(buf + off, len - off, "%c", 'A' + ch);
	}
	return (off - boff);
}

int
cmd_fill_argument(int flags, char **arg, char **arg2, int argc, char **argv)
{
	*arg = NULL;
	*arg2 = NULL;

	if (flags & CMD_ARG1) {
		if (argc != 1)
			return (-1);
		*arg = xstrdup(argv[0]);
		return (0);
	}

	if (flags & CMD_ARG01) {
		if (argc != 0 && argc != 1)
			return (-1);
		if (argc == 1)
			*arg = xstrdup(argv[0]);
		return (0);
	}

	if (flags & CMD_ARG2) {
		if (argc != 2)
			return (-1);
		*arg = xstrdup(argv[0]);
		*arg2 = xstrdup(argv[1]);
		return (0);
	}

	if (flags & CMD_ARG12) {
		if (argc != 1 && argc != 2)
			return (-1);
		*arg = xstrdup(argv[0]);
		if (argc == 2)
			*arg2 = xstrdup(argv[1]);
		return (0);
	}		

	if (argc != 0)
		return (-1);
	return (0);
}

void
cmd_target_init(struct cmd *self, unused int key)
{
	struct cmd_target_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->chflags = 0;
	data->target = NULL;
	data->arg = NULL;
	data->arg2 = NULL;
}

int
cmd_target_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_target_data	*data;
	const struct cmd_entry	*entry = self->entry;
	int			 opt;

	/* Don't use the entry version since it may be dependent on key. */
	cmd_target_init(self, 0);
	data = self->data;

	while ((opt = cmd_getopt(argc, argv, "t:", entry->chflags)) != -1) {
		if (cmd_parse_flags(opt, entry->chflags, &data->chflags) == 0)
			continue;
		switch (opt) {
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (cmd_fill_argument(
	    self->entry->flags, &data->arg, &data->arg2, argc, argv) != 0)
		goto usage;
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_target_free(struct cmd *self)
{
	struct cmd_target_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->arg != NULL)
		xfree(data->arg);
	if (data->arg2 != NULL)
		xfree(data->arg2);
	xfree(data);
}

size_t
cmd_target_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_target_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->chflags);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
 	if (off < len && data->arg2 != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg2);
	return (off);
}

void
cmd_srcdst_init(struct cmd *self, unused int key)
{
	struct cmd_srcdst_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->chflags = 0;
	data->src = NULL;
	data->dst = NULL;
	data->arg = NULL;
	data->arg2 = NULL;
}

int
cmd_srcdst_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_srcdst_data	*data;
	const struct cmd_entry	*entry = self->entry;
	int			 opt;

	cmd_srcdst_init(self, 0);
	data = self->data;

	while ((opt = cmd_getopt(argc, argv, "s:t:", entry->chflags)) != -1) {
		if (cmd_parse_flags(opt, entry->chflags, &data->chflags) == 0)
			continue;
		switch (opt) {
		case 's':
			if (data->src == NULL)
				data->src = xstrdup(optarg);
			break;
		case 't':
			if (data->dst == NULL)
				data->dst = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (cmd_fill_argument(
	    self->entry->flags, &data->arg, &data->arg2, argc, argv) != 0)
		goto usage;
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_srcdst_free(struct cmd *self)
{
	struct cmd_srcdst_data	*data = self->data;

	if (data->src != NULL)
		xfree(data->src);
	if (data->dst != NULL)
		xfree(data->dst);
	if (data->arg != NULL)
		xfree(data->arg);
	if (data->arg2 != NULL)
		xfree(data->arg2);
	xfree(data);
}

size_t
cmd_srcdst_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_srcdst_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->chflags);
	if (off < len && data->src != NULL)
		off += xsnprintf(buf + off, len - off, " -s %s", data->src);
	if (off < len && data->dst != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->dst);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
 	if (off < len && data->arg2 != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg2);
	return (off);
}

void
cmd_buffer_init(struct cmd *self, unused int key)
{
	struct cmd_buffer_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->chflags = 0;
	data->target = NULL;
	data->buffer = -1;
	data->arg = NULL;
	data->arg2 = NULL;
}

int
cmd_buffer_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_buffer_data	*data;
	const struct cmd_entry	*entry = self->entry;
	int			 opt, n;
	const char		*errstr;

	cmd_buffer_init(self, 0);
	data = self->data;

	while ((opt = cmd_getopt(argc, argv, "b:t:", entry->chflags)) != -1) {
		if (cmd_parse_flags(opt, entry->chflags, &data->chflags) == 0)
			continue;
		switch (opt) {
		case 'b':
			if (data->buffer == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "buffer %s", errstr);
					goto error;
				}
				data->buffer = n;
			}
			break;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (cmd_fill_argument(
	    self->entry->flags, &data->arg, &data->arg2, argc, argv) != 0)
		goto usage;
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

void
cmd_buffer_free(struct cmd *self)
{
	struct cmd_buffer_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->arg != NULL)
		xfree(data->arg);
	if (data->arg2 != NULL)
		xfree(data->arg2);
	xfree(data);
}

size_t
cmd_buffer_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_buffer_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->chflags);
	if (off < len && data->buffer != -1)
		off += xsnprintf(buf + off, len - off, " -b %d", data->buffer);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
 	if (off < len && data->arg2 != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg2);
	return (off);
}
