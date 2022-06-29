/* $OpenBSD$ */

/*
 * Copyright (c) 2021 Will <author@will.party>
 * Copyright (c) 2022 Jeff Chiang <pobomp@gmail.com>
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

/*
 * To efficiently store OSC-8 hyperlinks in extended cell attributes, assign
 * each hyperlink cell a numerical ID called the 'attribute ID'. This is
 * distinct from the string-valued ID described in the [specification][1],
 * henceforth referred to as the 'parameter ID'. Use a dual-layer tree to map
 * a URI / parameter ID pair to an attribute ID. Use a single-layer tree to do
 * the inverse; retrieve the URI and parameter ID given an attribute ID.
 *
 * The dual-layer tree for the forward mapping primarily ensures that each
 * unique URI is not duplicated in memory. The first layer maps URIs to nodes
 * containing second-layer trees, which map parameter IDs to attribute IDs.
 *
 * [1]: https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
 */


/* Second-layer tree for forward hyperlink mapping. */
struct id_to_inner {
	const char      *id;

	u_int		 inner;

	RB_ENTRY(id_to_inner)	entry;
};
RB_HEAD(id_to_inners, id_to_inner);

/* First-layer tree for forward hyperlink mapping. */
struct uri_to_id_tree {
	const char      *uri;

	u_int				 default_inner;
	struct id_to_inners	 inners_by_id;

	RB_ENTRY(uri_to_id_tree)	 entry;
};
RB_HEAD(uri_to_id_trees, uri_to_id_tree);

/* Tree for backward hyperlink mapping. */
struct inner_to_link {
	u_int		 inner;
	const char	*uri;
	const char	*id;

	RB_ENTRY(inner_to_link)	entry;
};
RB_HEAD(inner_to_links, inner_to_link);

struct hyperlinks {
	u_int	 ns;
	u_int	 next_inner;

	struct uri_to_id_trees	forward_mapping;
	struct inner_to_links	backward_mapping;
};

static int
uri_cmp(struct uri_to_id_tree *left, struct uri_to_id_tree *right);

static int
uri_cmp(struct uri_to_id_tree *left, struct uri_to_id_tree *right)
{
	return strcmp(left->uri, right->uri);
}

static int
id_cmp(struct id_to_inner *left,
    struct id_to_inner *right)
{
	return strcmp(left->id, right->id);
}

static int
attr_cmp(struct inner_to_link *left, struct inner_to_link *right)
{
	return left->inner - right->inner;
}

RB_PROTOTYPE_STATIC(uri_to_id_trees, uri_to_id_tree, entry,
    uri_cmp);
RB_PROTOTYPE_STATIC(id_to_inners, id_to_inner, entry,
    id_cmp);
RB_PROTOTYPE_STATIC(inner_to_links, inner_to_link, entry, attr_cmp);

RB_GENERATE_STATIC(uri_to_id_trees, uri_to_id_tree, entry,
    uri_cmp);
RB_GENERATE_STATIC(id_to_inners, id_to_inner, entry,
    id_cmp);
RB_GENERATE_STATIC(inner_to_links, inner_to_link, entry, attr_cmp);

static void
hyperlink_put_inverse(struct hyperlinks *hl, u_int *inner_dest,
    const char *uri, const char *id)
{
	struct inner_to_link	*inner_new;

	*inner_dest = hl->next_inner++;
	inner_new = xmalloc(sizeof *inner_new);
	inner_new->inner = *inner_dest;
	inner_new->uri = uri;
	inner_new->id = id;
	RB_INSERT(inner_to_links, &hl->backward_mapping, inner_new);
}

/*
 * Assume that id either has non-zero length or is NULL,
 * and that it's already copied from the input buffer if non-NULL.
 */
u_int
hyperlink_put(struct hyperlinks *hl, const char *uri, const char *id)
{
	struct uri_to_id_tree	 uri_search;
	struct uri_to_id_tree	*uri_found;
	struct id_to_inner	 id_search;
	struct id_to_inner	*id_found;

	uri_search.uri = uri;
	uri_found = RB_FIND(uri_to_id_trees,
	    &hl->forward_mapping, &uri_search);

	if (uri_found != NULL) {
		if (id == NULL) {
			if (uri_found->default_inner == 0) {
				/* Be sure to use the pre-copied URI from
				 * uri_found. */
				hyperlink_put_inverse(hl,
				    &uri_found->default_inner,
				    uri_found->uri, NULL);
			}
			return uri_found->default_inner;
		}

		id_search.id = id;
		id_found = RB_FIND(id_to_inners,
		    &uri_found->inners_by_id, &id_search);

		if (id_found != NULL) {
			free((void *)id);
			return id_found->inner;
		}
		goto same_uri_different_id;
	}

	uri_found = xmalloc(sizeof *uri_found);

	/* sanitize in case of invalid UTF-8 */
	utf8_stravis((char**)&uri_found->uri, uri, VIS_OCTAL|VIS_CSTYLE);

	RB_INIT(&uri_found->inners_by_id);
	RB_INSERT(uri_to_id_trees,
	    &hl->forward_mapping, uri_found);

	if (id == NULL) {
		/* Be sure to use the pre-copied URI from uri_found. */
		hyperlink_put_inverse(hl,
		    &uri_found->default_inner, uri_found->uri,
		    NULL);
		return uri_found->default_inner;
	}
	uri_found->default_inner = 0;

same_uri_different_id:
	id_found = xmalloc(sizeof *id_found);
	id_found->id = id;
	RB_INSERT(id_to_inners,
	    &uri_found->inners_by_id, id_found);
	/* Be sure to use the pre-copied value for URI. */
	hyperlink_put_inverse(hl, &id_found->inner,
	    uri_found->uri, id);
	return id_found->inner;
}

int
hyperlink_get(struct hyperlinks *hl, u_int inner, const char **uri_out,
    const char **id_out)
{
	struct inner_to_link	 inner_search;
	struct inner_to_link	*inner_found;

	inner_search.inner = inner;
	inner_found = RB_FIND(inner_to_links,
	    &hl->backward_mapping, &inner_search);

	if (inner_found == NULL)
		return 0;
	*uri_out = inner_found->uri;
	*id_out = inner_found->id;
	return 1;
}

void
hyperlink_init(struct hyperlinks **hl)
{
	static u_int    next_ns = 0;

	*hl = xmalloc(sizeof **hl);
	RB_INIT(&(*hl)->forward_mapping);
	RB_INIT(&(*hl)->backward_mapping);
	(*hl)->ns = next_ns++;
	(*hl)->next_inner = 1;
}

void
hyperlink_reset(struct hyperlinks *hl)
{
	struct uri_to_id_tree	*uri_curr;
	struct uri_to_id_tree	*uri_next;
	struct id_to_inner	*id_curr;
	struct id_to_inner	*id_next;
	struct inner_to_link	*inner_curr;
	struct inner_to_link	*inner_next;

	uri_curr = RB_MIN(uri_to_id_trees, &hl->forward_mapping);

	RB_FOREACH_SAFE(uri_curr, uri_to_id_trees, &hl->forward_mapping,
			uri_next) {
		RB_REMOVE(uri_to_id_trees, &hl->forward_mapping,
		    uri_curr);
		free((void *)uri_curr->uri);

		id_curr = RB_MIN(id_to_inners,
				&uri_curr->inners_by_id);

		RB_FOREACH_SAFE(id_curr, id_to_inners, &uri_curr->inners_by_id,
				id_next) {
				RB_REMOVE(id_to_inners,
						&uri_curr->inners_by_id, id_curr);
				free((void *)id_curr->id);
				free((void *)id_curr);
		}
		free(uri_curr);
	}

	inner_curr = RB_MIN(inner_to_links,
	    &hl->backward_mapping);

	RB_FOREACH_SAFE(inner_curr, inner_to_links, &hl->backward_mapping,
			inner_next) {
		RB_REMOVE(inner_to_links,
		    &hl->backward_mapping, inner_curr);
		free(inner_curr);
	}

	hl->next_inner = 1;
}

void
hyperlink_free(struct hyperlinks *hl)
{
	hyperlink_reset(hl);
	free(hl);
}

u_int
hyperlink_get_namespace(struct hyperlinks *hl)
{
	return hl->ns;
}
