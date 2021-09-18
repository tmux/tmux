
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
struct param_id_to_attr_id {
	const char      *param_id;

	u_int		 attr_id;

	RB_ENTRY(param_id_to_attr_id)	entry;
};
RB_HEAD(param_id_to_attr_ids, param_id_to_attr_id);

/* First-layer tree for forward hyperlink mapping. */
struct uri_to_param_id_tree {
	const char      *uri;

	u_int				 default_attr_id;
	struct param_id_to_attr_ids	 attr_ids_by_param_id;

	RB_ENTRY(uri_to_param_id_tree)	 entry;
};
RB_HEAD(uri_to_param_id_trees, uri_to_param_id_tree);

/* Tree for backward hyperlink mapping. */
struct attr_id_to_link {
	u_int		 attr_id;
	const char	*uri;
	const char	*param_id;

	RB_ENTRY(attr_id_to_link)	entry;
};
RB_HEAD(attr_id_to_links, attr_id_to_link);

struct hyperlinks {
	u_int	 ns;
	u_int	 next_attr_id;

	struct uri_to_param_id_trees	forward_mapping;
	struct attr_id_to_links		backward_mapping;
};

static int
uri_cmp(struct uri_to_param_id_tree *left, struct uri_to_param_id_tree *right);

static int
uri_cmp(struct uri_to_param_id_tree *left, struct uri_to_param_id_tree *right)
{
	return strcmp(left->uri, right->uri);
}

static int
param_id_cmp(struct param_id_to_attr_id *left,
    struct param_id_to_attr_id *right)
{
	return strcmp(left->param_id, right->param_id);
}

static int
attr_cmp(struct attr_id_to_link *left, struct attr_id_to_link *right)
{
	return (int)(left->attr_id - right->attr_id);
}

RB_PROTOTYPE_STATIC(uri_to_param_id_trees, uri_to_param_id_tree, entry,
    uri_cmp);
RB_PROTOTYPE_STATIC(param_id_to_attr_ids, param_id_to_attr_id, entry,
    param_id_cmp);
RB_PROTOTYPE_STATIC(attr_id_to_links, attr_id_to_link, entry, attr_cmp);

RB_GENERATE_STATIC(uri_to_param_id_trees, uri_to_param_id_tree, entry,
    uri_cmp);
RB_GENERATE_STATIC(param_id_to_attr_ids, param_id_to_attr_id, entry,
    param_id_cmp);
RB_GENERATE_STATIC(attr_id_to_links, attr_id_to_link, entry, attr_cmp);

static u_int
hyperlink_put_inverse(struct hyperlinks *hl, u_int *attr_id_dest,
    const char *uri, const char *param_id)
{
	struct attr_id_to_link	*attr_id_new;

	*attr_id_dest = hl->next_attr_id++;
	attr_id_new = xmalloc(sizeof *attr_id_new);
	attr_id_new->attr_id = *attr_id_dest;
	attr_id_new->uri = uri;
	attr_id_new->param_id = param_id;
	RB_INSERT(attr_id_to_links, &hl->backward_mapping, attr_id_new);
	return *attr_id_dest;
}

/*
 * Assume that param_id either has non-zero length or is NULL,
 * and that it's already copied from the input buffer if non-NULL.
 */
u_int
hyperlink_put(struct hyperlinks *hl, const char *uri, const char *param_id)
{
	struct uri_to_param_id_tree	 uri_search;
	struct uri_to_param_id_tree	*uri_found;
	struct param_id_to_attr_id	 param_id_search;
	struct param_id_to_attr_id	*param_id_found;

	uri_search.uri = uri;
	uri_found = RB_FIND(uri_to_param_id_trees,
	    &hl->forward_mapping, &uri_search);

	if (uri_found != NULL) {
		if (param_id == NULL) {
			if (uri_found->default_attr_id == 0)
				/* Be sure to use the pre-copied URI from
				 * uri_found. */
				return hyperlink_put_inverse(hl,
				    &uri_found->default_attr_id,
				    uri_found->uri, NULL);
			return uri_found->default_attr_id;
		}

		param_id_search.param_id = param_id;
		param_id_found = RB_FIND(param_id_to_attr_ids,
		    &uri_found->attr_ids_by_param_id, &param_id_search);

		if (param_id_found != NULL) {
			free(param_id);
			return param_id_found->attr_id;
		}
		goto same_uri_different_param_id;
	}

	uri_found = xmalloc(sizeof *uri_found);
	uri_found->uri = xstrdup(uri);
	RB_INIT(&uri_found->attr_ids_by_param_id);
	RB_INSERT(uri_to_param_id_trees,
	    &hl->forward_mapping, uri_found);

	if (param_id == NULL)
		/* Be sure to use the pre-copied URI from uri_found. */
		return hyperlink_put_inverse(hl,
		    &uri_found->default_attr_id, uri_found->uri,
		    NULL);
	uri_found->default_attr_id = 0;

same_uri_different_param_id:
	param_id_found = xmalloc(sizeof *param_id_found);
	param_id_found->param_id = param_id;
	RB_INSERT(param_id_to_attr_ids,
	    &uri_found->attr_ids_by_param_id, param_id_found);
	/* Be sure to use the pre-copied value for URI. */
	return hyperlink_put_inverse(hl, &param_id_found->attr_id,
	    uri_found->uri, param_id);
}

int
hyperlink_get(const struct hyperlinks *hl, u_int attr_id, const char **uri_out,
    const char **param_id_out)
{
	struct attr_id_to_link	 attr_id_search;
	struct attr_id_to_link	*attr_id_found;

	attr_id_search.attr_id = attr_id;
	attr_id_found = RB_FIND(attr_id_to_links,
	    &hl->backward_mapping, &attr_id_search);

	if (attr_id_found == NULL)
		return 0;
	*uri_out = attr_id_found->uri;
	*param_id_out = attr_id_found->param_id;
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
	(*hl)->next_attr_id = 1;
}

/*
 * Each hyperlink tree has a 'namespace' used to prefix parameter IDs when
 * rendering, so that links from different trees basically never have the same
 * parameter ID. It's not a big deal if there are rare collisions.
 */
char *
hyperlink_write_namespaced(struct hyperlinks *hl, char *param_id,
    const char *raw_param_id, size_t raw_param_id_length)
{
	param_id = xrealloc(param_id, raw_param_id_length + 5);
	snprintf(param_id, raw_param_id_length + 5, "%.3X.%s", hl->ns,
	    raw_param_id);
	return param_id;
}

void
hyperlink_reset(struct hyperlinks *hl)
{
	struct uri_to_param_id_tree	*uri_curr;
	struct uri_to_param_id_tree	*uri_next;
	struct param_id_to_attr_id	*param_id_curr;
	struct param_id_to_attr_id	*param_id_next;
	struct attr_id_to_link	*attr_id_curr;
	struct attr_id_to_link	*attr_id_next;

	uri_curr = RB_MIN(uri_to_param_id_trees, &hl->forward_mapping);
	while (uri_curr != NULL) {
		uri_next = RB_NEXT(uri_to_param_id_trees,
		    &hl->forward_mapping, uri_curr);
		RB_REMOVE(uri_to_param_id_trees, &hl->forward_mapping,
		    uri_curr);
		free(uri_curr->uri);

		param_id_curr = RB_MIN(param_id_to_attr_ids, &uri_curr->attr_ids_by_param_id);
		while (param_id_curr != NULL) {
			param_id_next = RB_NEXT(param_id_to_attr_ids,
			    &uri_curr->attr_ids_by_param_id, param_id_curr);
			RB_REMOVE(param_id_to_attr_ids,
			    &uri_curr->attr_ids_by_param_id, param_id_curr);
			free(param_id_curr->param_id);
			free(param_id_curr);
			param_id_curr = param_id_next;
		}

		free(uri_curr);
		uri_curr = uri_next;
	}

	attr_id_curr = RB_MIN(attr_id_to_links,
	    &hl->backward_mapping);
	while (attr_id_curr != NULL) {
		attr_id_next = RB_NEXT(attr_id_to_links,
		    &hl->backward_mapping, attr_id_curr);
		RB_REMOVE(attr_id_to_links,
		    &hl->backward_mapping, attr_id_curr);
		free(attr_id_curr);
		attr_id_curr = attr_id_next;
	}

	hl->next_attr_id = 1;
}

void
hyperlink_free(struct hyperlinks *hl)
{
	hyperlink_reset(hl);
	free(hl);
}
