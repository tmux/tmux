/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/time.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Set of paste buffers. Note that paste buffer data is not necessarily a C
 * string!
 */

struct paste_buffer {
	char		*data;
	size_t		 size;

	char		*name;
	int		 automatic;
	u_int		 order;

	RB_ENTRY(paste_buffer) name_entry;
	RB_ENTRY(paste_buffer) time_entry;
};

u_int	paste_next_index;
u_int	paste_next_order;
u_int	paste_num_automatic;
RB_HEAD(paste_name_tree, paste_buffer) paste_by_name;
RB_HEAD(paste_time_tree, paste_buffer) paste_by_time;

int paste_cmp_names(const struct paste_buffer *, const struct paste_buffer *);
RB_PROTOTYPE(paste_name_tree, paste_buffer, name_entry, paste_cmp_names);
RB_GENERATE(paste_name_tree, paste_buffer, name_entry, paste_cmp_names);

int paste_cmp_times(const struct paste_buffer *, const struct paste_buffer *);
RB_PROTOTYPE(paste_time_tree, paste_buffer, time_entry, paste_cmp_times);
RB_GENERATE(paste_time_tree, paste_buffer, time_entry, paste_cmp_times);

int
paste_cmp_names(const struct paste_buffer *a, const struct paste_buffer *b)
{
	return (strcmp(a->name, b->name));
}

int
paste_cmp_times(const struct paste_buffer *a, const struct paste_buffer *b)
{
	if (a->order > b->order)
		return (-1);
	if (a->order < b->order)
		return (1);
	return (0);
}

/* Get paste buffer name. */
const char *
paste_buffer_name(struct paste_buffer *pb)
{
	return (pb->name);
}

/* Get paste buffer data. */
const char *
paste_buffer_data(struct paste_buffer *pb, size_t *size)
{
	if (size != NULL)
		*size = pb->size;
	return (pb->data);
}

/* Walk paste buffers by name. */
struct paste_buffer *
paste_walk(struct paste_buffer *pb)
{
	if (pb == NULL)
		return (RB_MIN(paste_time_tree, &paste_by_time));
	return (RB_NEXT(paste_time_tree, &paste_by_time, pb));
}

/* Get the most recent automatic buffer. */
struct paste_buffer *
paste_get_top(const char **name)
{
	struct paste_buffer	*pb;

	pb = RB_MIN(paste_time_tree, &paste_by_time);
	if (pb == NULL)
		return (NULL);
	if (name != NULL)
		*name = pb->name;
	return (pb);
}

/* Get a paste buffer by name. */
struct paste_buffer *
paste_get_name(const char *name)
{
	struct paste_buffer	pbfind;

	if (name == NULL || *name == '\0')
		return (NULL);

	pbfind.name = (char *)name;
	return (RB_FIND(paste_name_tree, &paste_by_name, &pbfind));
}

/* Free a paste buffer. */
void
paste_free(struct paste_buffer *pb)
{
	RB_REMOVE(paste_name_tree, &paste_by_name, pb);
	RB_REMOVE(paste_time_tree, &paste_by_time, pb);
	if (pb->automatic)
		paste_num_automatic--;

	free(pb->data);
	free(pb->name);
	free(pb);
}

/*
 * Add an automatic buffer, freeing the oldest automatic item if at limit. Note
 * that the caller is responsible for allocating data.
 */
void
paste_add(char *data, size_t size)
{
	struct paste_buffer	*pb, *pb1;
	u_int			 limit;

	if (size == 0)
		return;

	limit = options_get_number(global_options, "buffer-limit");
	RB_FOREACH_REVERSE_SAFE(pb, paste_time_tree, &paste_by_time, pb1) {
		if (paste_num_automatic < limit)
			break;
		if (pb->automatic)
			paste_free(pb);
	}

	pb = xmalloc(sizeof *pb);

	pb->name = NULL;
	do {
		free(pb->name);
		xasprintf(&pb->name, "buffer%04u", paste_next_index);
		paste_next_index++;
	} while (paste_get_name(pb->name) != NULL);

	pb->data = data;
	pb->size = size;

	pb->automatic = 1;
	paste_num_automatic++;

	pb->order = paste_next_order++;
	RB_INSERT(paste_name_tree, &paste_by_name, pb);
	RB_INSERT(paste_time_tree, &paste_by_time, pb);
}

/* Rename a paste buffer. */
int
paste_rename(const char *oldname, const char *newname, char **cause)
{
	struct paste_buffer	*pb, *pb_new;

	if (cause != NULL)
		*cause = NULL;

	if (oldname == NULL || *oldname == '\0') {
		if (cause != NULL)
			*cause = xstrdup("no buffer");
		return (-1);
	}
	if (newname == NULL || *newname == '\0') {
		if (cause != NULL)
			*cause = xstrdup("new name is empty");
		return (-1);
	}

	pb = paste_get_name(oldname);
	if (pb == NULL) {
		if (cause != NULL)
			xasprintf(cause, "no buffer %s", oldname);
		return (-1);
	}

	pb_new = paste_get_name(newname);
	if (pb_new != NULL) {
		if (cause != NULL)
			xasprintf(cause, "buffer %s already exists", newname);
		return (-1);
	}

	RB_REMOVE(paste_name_tree, &paste_by_name, pb);

	free(pb->name);
	pb->name = xstrdup(newname);

	if (pb->automatic)
		paste_num_automatic--;
	pb->automatic = 0;

	RB_INSERT(paste_name_tree, &paste_by_name, pb);

	return (0);
}

/*
 * Add or replace an item in the store. Note that the caller is responsible for
 * allocating data.
 */
int
paste_set(char *data, size_t size, const char *name, char **cause)
{
	struct paste_buffer	*pb, *old;

	if (cause != NULL)
		*cause = NULL;

	if (size == 0) {
		free(data);
		return (0);
	}
	if (name == NULL) {
		paste_add(data, size);
		return (0);
	}

	if (*name == '\0') {
		if (cause != NULL)
			*cause = xstrdup("empty buffer name");
		return (-1);
	}

	pb = xmalloc(sizeof *pb);

	pb->name = xstrdup(name);

	pb->data = data;
	pb->size = size;

	pb->automatic = 0;
	pb->order = paste_next_order++;

	if ((old = paste_get_name(name)) != NULL)
		paste_free(old);

	RB_INSERT(paste_name_tree, &paste_by_name, pb);
	RB_INSERT(paste_time_tree, &paste_by_time, pb);

	return (0);
}

/* Convert start of buffer into a nice string. */
char *
paste_make_sample(struct paste_buffer *pb)
{
	char		*buf;
	size_t		 len, used;
	const int	 flags = VIS_OCTAL|VIS_TAB|VIS_NL;
	const size_t	 width = 200;

	len = pb->size;
	if (len > width)
		len = width;
	buf = xreallocarray(NULL, len, 4 + 4);

	used = utf8_strvis(buf, pb->data, len, flags);
	if (pb->size > width || used > width)
		strlcpy(buf + width, "...", 4);
	return (buf);
}
