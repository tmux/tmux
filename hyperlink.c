
#include <string.h>

#include "tmux.h"

RB_GENERATE(hyperlink_uri_to_param_id_trees, hyperlink_uri_to_param_id_tree,
	entry, hyperlink_uri_to_param_id_tree_cmp);
RB_GENERATE(hyperlink_param_id_to_attr_ids, hyperlink_param_id_to_attr_id,
	entry, hyperlink_param_id_to_attr_id_cmp);
RB_GENERATE(hyperlink_attr_id_to_links, hyperlink_attr_id_to_link,
	entry, hyperlink_attr_id_to_link_cmp);

int
hyperlink_param_id_to_attr_id_cmp(struct hyperlink_param_id_to_attr_id *left,
    struct hyperlink_param_id_to_attr_id *right)
{
	return strcmp(left->param_id, right->param_id);
}

int
hyperlink_uri_to_param_id_tree_cmp(struct hyperlink_uri_to_param_id_tree *left,
    struct hyperlink_uri_to_param_id_tree *right)
{
	return strcmp(left->uri, right->uri);
}

int
hyperlink_attr_id_to_link_cmp(struct hyperlink_attr_id_to_link *left,
    struct hyperlink_attr_id_to_link *right)
{
	return (int)(left->attr_id - right->attr_id);
}

static u_int
hyperlink_put_inverse(struct window_pane *wp, u_int *attr_id_dest,
    const char *uri, const char *param_id)
{
	struct hyperlink_attr_id_to_link	*attr_id_new;

	*attr_id_dest = wp->hyperlink_next_attr_id++;
	attr_id_new = xmalloc(sizeof *attr_id_new);
	attr_id_new->attr_id = *attr_id_dest;
	attr_id_new->uri = uri;
	attr_id_new->param_id = param_id;
	RB_INSERT(hyperlink_attr_id_to_links, &wp->hyperlink_backward_mapping,
	    attr_id_new);
	return *attr_id_dest;
}

/* Assume that `param_id` either has non-zero length or is NULL. */
u_int
hyperlink_put(struct window_pane *wp, const char *uri, const char *param_id)
{
	struct hyperlink_uri_to_param_id_tree	 uri_search;
	struct hyperlink_uri_to_param_id_tree	*uri_found;
	struct hyperlink_param_id_to_attr_id	 param_id_search;
	struct hyperlink_param_id_to_attr_id	*param_id_found;

	uri_search.uri = uri;
	uri_found = RB_FIND(hyperlink_uri_to_param_id_trees,
	    &wp->hyperlink_forward_mapping, &uri_search);

	if (uri_found != NULL) {
		if (param_id == NULL) {
			if (uri_found->default_attr_id == 0)
				/* Be sure to use the pre-copied URI from
				 * `uri_found`. */
				return hyperlink_put_inverse(wp,
				    &uri_found->default_attr_id,
				    uri_found->uri, NULL);
			return uri_found->default_attr_id;
		}

		param_id_search.param_id = param_id;
		param_id_found = RB_FIND(hyperlink_param_id_to_attr_ids,
		    &uri_found->attr_ids_by_param_id, &param_id_search);

		if (param_id_found != NULL)
			return param_id_found->attr_id;
		goto same_uri_different_param_id;
	}

	uri_found = xmalloc(sizeof *uri_found);
	uri_found->uri = xstrdup(uri);
	RB_INIT(&uri_found->attr_ids_by_param_id);
	RB_INSERT(hyperlink_uri_to_param_id_trees,
	    &wp->hyperlink_forward_mapping, uri_found);

	if (param_id == NULL)
		/* Be sure to use the pre-copied URI from `uri_found`. */
		return hyperlink_put_inverse(wp,
		    &uri_found->default_attr_id, uri_found->uri,
		    NULL);
	uri_found->default_attr_id = 0;

same_uri_different_param_id:
	param_id_found = xmalloc(sizeof *param_id_found);
	param_id_found->param_id = xstrdup(param_id);
	RB_INSERT(hyperlink_param_id_to_attr_ids,
	    &uri_found->attr_ids_by_param_id, param_id_found);
	/* Be sure to use pre-copied values for URI and parameter ID. */
	return hyperlink_put_inverse(wp, &param_id_found->attr_id,
	    uri_found->uri, param_id_found->param_id);
}

int
hyperlink_get(struct window_pane *wp, u_int attr_id, const char **uri_out,
    const char **param_id_out)
{
	struct hyperlink_attr_id_to_link	 attr_id_search;
	struct hyperlink_attr_id_to_link	*attr_id_found;

	attr_id_search.attr_id = attr_id;
	attr_id_found = RB_FIND(hyperlink_attr_id_to_links,
	    &wp->hyperlink_backward_mapping, &attr_id_search);

	if (attr_id_found == NULL)
		return 0;
	*uri_out = attr_id_found->uri;
	*param_id_out = attr_id_found->param_id;
	return 1;
}

void
hyperlink_destroy_trees(struct window_pane *wp) {
	struct hyperlink_uri_to_param_id_tree	*uri_curr;
	struct hyperlink_uri_to_param_id_tree	*uri_next;
	struct hyperlink_param_id_to_attr_id	*param_id_curr;
	struct hyperlink_param_id_to_attr_id	*param_id_next;
	struct hyperlink_attr_id_to_link	*attr_id_curr;
	struct hyperlink_attr_id_to_link	*attr_id_next;

	uri_curr = RB_MIN(hyperlink_uri_to_param_id_trees,
	    &wp->hyperlink_forward_mapping);
	while (uri_curr != NULL) {
		uri_next = RB_NEXT(hyperlink_uri_to_param_id_trees,
		    &wp->hyperlink_forward_mapping, uri_curr);
		RB_REMOVE(hyperlink_uri_to_param_id_trees,
		    &wp->hyperlink_forward_mapping, uri_curr);
		free(uri_curr->uri);

		param_id_curr = RB_MIN(hyperlink_param_id_to_attr_ids,
		    &uri_curr->attr_ids_by_param_id);
		while (param_id_curr != NULL) {
			param_id_next = RB_NEXT(hyperlink_param_id_to_attr_ids,
			    &uri_curr->attr_ids_by_param_id, param_id_curr);
			RB_REMOVE(hyperlink_param_id_to_attr_ids,
			    &uri_curr->attr_ids_by_param_id, param_id_curr);
			free(param_id_curr->param_id);
			free(param_id_curr);
			param_id_curr = param_id_next;
		}

		free(uri_curr);
		uri_curr = uri_next;
	}

	attr_id_curr = RB_MIN(hyperlink_attr_id_to_links,
	    &wp->hyperlink_backward_mapping);
	while (attr_id_curr != NULL) {
		attr_id_next = RB_NEXT(hyperlink_attr_id_to_links,
		    &wp->hyperlink_backward_mapping, attr_id_curr);
		RB_REMOVE(hyperlink_attr_id_to_links,
		    &wp->hyperlink_backward_mapping, attr_id_curr);
		free(attr_id_curr);
		attr_id_curr = attr_id_next;
	}
}
