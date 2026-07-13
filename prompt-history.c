/* $OpenBSD: prompt-history.c,v 1.1 2026/06/25 11:39:11 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static char	*prompt_find_history_file(void);
static void	 prompt_add_typed_history(char *);

/* Prompt history. */
static char	**prompt_hlist[PROMPT_NTYPES];
static u_int	  prompt_hsize[PROMPT_NTYPES];

/* Find the history file to load/save from/to. */
static char *
prompt_find_history_file(void)
{
	const char	*home, *history_file;
	char		*path;

	history_file = options_get_string(global_options, "history-file");
	if (*history_file == '\0')
		return (NULL);
	if (*history_file == '/')
		return (xstrdup(history_file));

	if (history_file[0] != '~' || history_file[1] != '/')
		return (NULL);
	if ((home = find_home()) == NULL)
		return (NULL);
	xasprintf(&path, "%s%s", home, history_file + 1);
	return (path);
}

/* Add loaded history item to the appropriate list. */
static void
prompt_add_typed_history(char *line)
{
	char			*typestr;
	enum prompt_type	 type = PROMPT_TYPE_INVALID;

	typestr = strsep(&line, ":");
	if (line != NULL)
		type = prompt_type(typestr);
	if (type == PROMPT_TYPE_INVALID) {
		/*
		 * Invalid types are not expected, but this provides backward
		 * compatibility with old history files.
		 */
		if (line != NULL)
			*(--line) = ':';
		prompt_add_history(typestr, PROMPT_TYPE_COMMAND);
	} else
		prompt_add_history(line, type);
}

/* Load prompt history from file. */
void
prompt_load_history(void)
{
	FILE	*f;
	char	*history_file, *line, *tmp;
	size_t	 length;

	if ((history_file = prompt_find_history_file()) == NULL)
		return;
	log_debug("loading history from %s", history_file);

	f = fopen(history_file, "r");
	if (f == NULL) {
		log_debug("%s: %s", history_file, strerror(errno));
		free(history_file);
		return;
	}
	free(history_file);

	for (;;) {
		if ((line = fgetln(f, &length)) == NULL)
			break;

		if (length > 0) {
			if (line[length - 1] == '\n') {
				line[length - 1] = '\0';
				prompt_add_typed_history(line);
			} else {
				tmp = xmalloc(length + 1);
				memcpy(tmp, line, length);
				tmp[length] = '\0';
				prompt_add_typed_history(tmp);
				free(tmp);
			}
		}
	}
	fclose(f);
}

/* Save prompt history to file. */
void
prompt_save_history(void)
{
	FILE	*f;
	u_int	 i, type;
	char	*history_file;

	if ((history_file = prompt_find_history_file()) == NULL)
		return;
	log_debug("saving history to %s", history_file);

	f = fopen(history_file, "w");
	if (f == NULL) {
		log_debug("%s: %s", history_file, strerror(errno));
		free(history_file);
		return;
	}
	free(history_file);

	for (type = 0; type < PROMPT_NTYPES; type++) {
		for (i = 0; i < prompt_hsize[type]; i++) {
			fputs(prompt_type_string(type), f);
			fputc(':', f);
			fputs(prompt_hlist[type][i], f);
			fputc('\n', f);
		}
	}
	fclose(f);

}

/* Get previous line from the history. */
const char *
prompt_up_history(u_int *idx, u_int type)
{
	/*
	 * History runs from 0 to size - 1. Index is from 0 to size. Zero is
	 * empty.
	 */

	if (type >= PROMPT_NTYPES)
		return (NULL);
	if (prompt_hsize[type] == 0 || idx[type] == prompt_hsize[type])
		return (NULL);
	idx[type]++;
	return (prompt_hlist[type][prompt_hsize[type] - idx[type]]);
}

/* Get next line from the history. */
const char *
prompt_down_history(u_int *idx, u_int type)
{
	if (type >= PROMPT_NTYPES)
		return ("");
	if (prompt_hsize[type] == 0 || idx[type] == 0)
		return ("");
	idx[type]--;
	if (idx[type] == 0)
		return ("");
	return (prompt_hlist[type][prompt_hsize[type] - idx[type]]);
}

/* Add line to the history. */
void
prompt_add_history(const char *line, u_int type)
{
	u_int	i, oldsize, newsize, freecount, hlimit, new = 1;
	size_t	movesize;

	if (type >= PROMPT_NTYPES)
		return;

	oldsize = prompt_hsize[type];
	if (oldsize > 0 &&
	    strcmp(prompt_hlist[type][oldsize - 1], line) == 0)
		new = 0;

	hlimit = options_get_number(global_options, "prompt-history-limit");
	if (hlimit > oldsize) {
		if (new == 0)
			return;
		newsize = oldsize + new;
	} else {
		newsize = hlimit;
		freecount = oldsize + new - newsize;
		if (freecount > oldsize)
			freecount = oldsize;
		if (freecount == 0)
			return;
		for (i = 0; i < freecount; i++)
			free(prompt_hlist[type][i]);
		movesize = (oldsize - freecount) *
		    sizeof *prompt_hlist[type];
		if (movesize > 0) {
			memmove(&prompt_hlist[type][0],
			    &prompt_hlist[type][freecount], movesize);
		}
	}

	if (newsize == 0) {
		free(prompt_hlist[type]);
		prompt_hlist[type] = NULL;
	} else if (newsize != oldsize) {
		prompt_hlist[type] =
		    xreallocarray(prompt_hlist[type], newsize,
			sizeof *prompt_hlist[type]);
	}

	if (new == 1 && newsize > 0)
		prompt_hlist[type][newsize - 1] = xstrdup(line);
	prompt_hsize[type] = newsize;
}

/* Get history size. */
u_int
prompt_history_size(enum prompt_type type)
{
	if (type >= PROMPT_NTYPES)
		return (0);
	return (prompt_hsize[type]);
}

/* Get history entry. */
const char *
prompt_history_get(enum prompt_type type, u_int idx)
{
	if (type >= PROMPT_NTYPES)
		return (NULL);
	if (idx >= prompt_hsize[type])
		return (NULL);
	return (prompt_hlist[type][idx]);
}

/* Clear prompt history. */
void
prompt_history_clear(enum prompt_type type)
{
	u_int	idx;

	if (type >= PROMPT_NTYPES)
		return;
	for (idx = 0; idx < prompt_hsize[type]; idx++)
		free(prompt_hlist[type][idx]);
	free(prompt_hlist[type]);
	prompt_hlist[type] = NULL;
	prompt_hsize[type] = 0;
}
