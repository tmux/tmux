/* $OpenBSD: layout-custom.c,v 1.38 2026/07/16 12:36:58 nicm Exp $ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Layouts can be represented as strings in a JSON format (v2). The legacy
 * format (v1) will be deprecated in the future and should no longer be used.
 *
 * The current (v2) format is JSON. The top level has two keys:
 *    "V": version number, currently 2
 *    "L": root layout cell
 * Each cell is an object with:
 *    "t": cell type:
 *        "h": horizontal
 *        "v": vertical
 *        "p": pane
 *    "w": cell width
 *    "h": cell height
 *    "x": horizontal position
 *    "y": vertical position
 *  If the cell is a node cell (with child cells), it additionally has:
 *    "c": array of child cells
 *  If the cell is a leaf cell (that is, containing a pane and no child cells),
 *  it additionally has:
 *    "i": pane ID as %n
 *    "l": index into last panes list, if not the active pane
 *    "a": true if the active pane
 *    "z": z-index, if a floating pane
 */

/* Maximums */
#define LAYOUT_STRING_MAX	8192
#define TOKENS_MAX		(LAYOUT_STRING_MAX)

/* Layout string. */
struct layout_string {
	char	*write;
	char	 dat[LAYOUT_STRING_MAX];
};

/* Layout string view. */
struct layout_string_view {
	const char	*ptr;
	int		 len;
};

/* Token types. */
enum layout_token_type {
	TOK_OPENOBJECT,
	TOK_CLOSEOBJECT,
	TOK_OPENARRAY,
	TOK_CLOSEARRAY,
	TOK_COMMA,
	TOK_COLON,
	TOK_QUOTE,
	TOK_VALUE,
	TOK_EOF
};

/* Layout token. */
struct layout_token {
	enum layout_token_type		type;
	struct layout_string_view	val;
};

/* Layout tokens. */
struct layout_tokens {
	struct layout_token	*toks;
	int			 size;
};

/* Node type. */
enum layout_node_type {
	NODE_STRING,
	NODE_NUMBER,
	NODE_BOOLEAN,
	NODE_OBJECT,
	NODE_ARRAY
};

/* Node queue. */
TAILQ_HEAD(layout_nodes, layout_node);

/* Node. */
struct layout_node {
	struct layout_node		*parent;
	struct layout_string_view	 key;
	enum layout_node_type		 type;
	union {
		struct layout_string_view	lsv;
		int64_t				num;
		int				bool;
		struct layout_nodes		fields;
	} val;
	TAILQ_ENTRY(layout_node)	 entry;
};

static struct layout_tokens	*layout_tokenize_input(const char *);
static int			 layout_tokenize_value(struct layout_tokens *,
				     const char *, struct layout_string_view *);
static struct layout_tokens	*layout_tokens_create(void);
static void			 layout_tokens_destroy(struct layout_tokens *);
static int			 layout_tokens_push(struct layout_tokens *,
				     enum layout_token_type,
				     struct layout_string_view *);
static const struct layout_token *layout_tokens_last(struct layout_tokens *);
static struct layout_node	*layout_node_create(struct layout_node *,
				     enum layout_node_type,
				     struct layout_string_view *, const void *);
static void			 layout_node_destroy(struct layout_node *);
static void			 layout_node_assign(struct layout_node *,
				     const void *);
static struct layout_node *layout_parse_layout_tokens(struct layout_tokens **);
static int			 layout_parse_key(struct layout_token **,
				     struct layout_string_view *);
static struct layout_node	*layout_parse_object(struct layout_token **,
				     struct layout_node *,
				     struct layout_string_view *);
static struct layout_node	*layout_parse_array(struct layout_token **,
				     struct layout_node *,
				     struct layout_string_view *);
static struct layout_node	*layout_parse_string(struct layout_token **,
				     struct layout_node *,
				     struct layout_string_view *);
static struct layout_node	*layout_parse_number(struct layout_token **,
				     struct layout_node *,
				     struct layout_string_view *);
static struct layout_node	*layout_parse_boolean(struct layout_token **,
				     struct layout_node *,
				     struct layout_string_view *);
static int			 layout_key_is_eq(const struct layout_node *,
				     const char *);
static int			 layout_val_is_eq(const struct layout_node *,
				     const void *);
static struct layout_cell	*layout_evaluate_nodes(struct layout_node *,
				     int);
static struct layout_cell	*layout_evaluate_layout(struct layout_node *,
				     struct layout_cell *);

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *);
static int			 layout_append(struct layout_cell *,
				     struct layout_string *, int);
static struct layout_cell	*layout_construct(const char *, char **);
static void			 layout_assign(struct window_pane **,
				     struct layout_cell *);

/* Wrapper for string views with strcmp semantics. */
static int
lsvcmp(const struct layout_string_view *lsv, const char *s)
{
	int	len;

	len = lsv->len - (int)strlen(s);
	if (len < 0)
		return (-1);
	if (len > 0)
		return (1);

	return (memcmp(lsv->ptr, s, lsv->len));
}

/* Tokenize the layout string. */
static struct layout_tokens *
layout_tokenize_input(const char *input)
{
	struct layout_tokens		*tokens = layout_tokens_create();
	enum layout_token_type		 type;
	struct layout_string_view	 lsv = { 0 };
	int				 scan;

	while (*input != '\0') {
		if (tokens->size >= TOKENS_MAX)
			goto fail;
		switch (*input) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			input++;
			continue;
		case '{':
			type = TOK_OPENOBJECT;
			break;
		case '}':
			type = TOK_CLOSEOBJECT;
			break;
		case '[':
			type = TOK_OPENARRAY;
			break;
		case ']':
			type = TOK_CLOSEARRAY;
			break;
		case '"':
			type = TOK_QUOTE;
			break;
		case ':':
			type = TOK_COLON;
			break;
		case ',':
			type = TOK_COMMA;
			break;
		default:
			type = TOK_VALUE;
			scan = layout_tokenize_value(tokens, input, &lsv);
			if (scan == -1)
				goto fail;
			input += scan - 1;
		}
		if (layout_tokens_push(tokens, type, &lsv) != 0)
			goto fail;

		lsv.ptr = NULL;
		lsv.len = 0;
		input++;
	}
	if (layout_tokens_push(tokens, TOK_EOF, NULL) != 0)
		goto fail;

	return (tokens);
fail:
	layout_tokens_destroy(tokens);
	return (NULL);
}

/*
 * Tokenize a value from the input string. Strings are terminated by a '"', and
 * numbers/booleans are terminated by a ',', ']', or '}'.
 */
static int
layout_tokenize_value(struct layout_tokens *tokens, const char *input,
    struct layout_string_view *lsv)
{
	const struct layout_token	*prev = layout_tokens_last(tokens);
	int				 scan = 0;

	if (prev == NULL)
		return (-1);
	if (prev->type == TOK_QUOTE) {
		do {
			if (input[scan] == '\0')
				return (-1);
			scan++;
		} while (input[scan] != '"' || input[scan - 1] == '\\');
	} else if (prev->type == TOK_COLON) {
		do {
			if (input[scan] == '\0')
				return (-1);
			scan++;
		} while (input[scan] != ']' && input[scan] != '}' &&
		    input[scan] != ',');
	} else
		return (-1);

	lsv->ptr = input;
	lsv->len = scan;

	return (scan);
}

/* Create a new token container. */
static struct layout_tokens *
layout_tokens_create(void)
{
	struct layout_tokens	*tokens;

	tokens = xmalloc(sizeof *tokens);
	tokens->size = 0;
	tokens->toks = xmalloc(sizeof *tokens->toks * TOKENS_MAX);

	return (tokens);
}

/*
 * Destroy a token container, freeing remaining memory in the case of a parsing
 * failure.
 */
static void
layout_tokens_destroy(struct layout_tokens *tokens)
{
	free(tokens->toks);
	tokens->toks = NULL;
	free(tokens);
}

/* Add a token to the token container. */
static int
layout_tokens_push(struct layout_tokens *tokens, enum layout_token_type type,
    struct layout_string_view *lsv)
{
	struct layout_token	*tok;

	if (tokens->size >= TOKENS_MAX)
		return (-1);

	tok = &tokens->toks[tokens->size++];
	tok->type = type;
	if (lsv != NULL) {
		tok->val.ptr = lsv->ptr;
		tok->val.len = lsv->len;
	}

	return (0);
}

/* Returns a reference to the last token. */
static const struct layout_token *
layout_tokens_last(struct layout_tokens *tokens)
{
	if (tokens->size == 0)
		return (NULL);
	return (&tokens->toks[tokens->size - 1]);
}

/* Create a node and assign given values. */
static struct layout_node *
layout_node_create(struct layout_node *parent, enum layout_node_type type,
    struct layout_string_view *key, const void *val)
{
	struct layout_node	*node;

	node = xcalloc(1, sizeof *node);
	node->parent = parent;
	if (key != NULL) {
		node->key.ptr = key->ptr;
		node->key.len = key->len;
	}
	node->type = type;
	TAILQ_INIT(&node->val.fields);
	if (val != NULL)
		layout_node_assign(node, val);

	return (node);
}

/* Assign a value to a node. */
static void
layout_node_assign(struct layout_node *node, const void *val)
{
	struct layout_node		*child;
	struct layout_string_view	*lsv;

	switch (node->type) {
	case NODE_STRING:
		lsv = (struct layout_string_view *)val;
		node->val.lsv.ptr = lsv->ptr;
		node->val.lsv.len = lsv->len;
		break;
	case NODE_NUMBER:
		node->val.num = *(int64_t *)val;
		break;
	case NODE_BOOLEAN:
		node->val.bool = *(int *)val;
		break;
	case NODE_OBJECT:
	case NODE_ARRAY:
		child = (struct layout_node *)val;
		TAILQ_INSERT_TAIL(&node->val.fields, child, entry);
		break;
	default:
		fatalx("unknown node type");
	}
}

/* Destroy a node and all of the node's fields. */
static void
layout_node_destroy(struct layout_node *node)
{
	struct layout_node	*field;

	if (node == NULL)
		return;

	switch (node->type) {
	case NODE_STRING:
	case NODE_NUMBER:
	case NODE_BOOLEAN:
		break;
	case NODE_OBJECT:
	case NODE_ARRAY:
		while (!TAILQ_EMPTY(&node->val.fields)) {
			field = TAILQ_FIRST(&node->val.fields);
			TAILQ_REMOVE(&node->val.fields, field, entry);
			layout_node_destroy(field);
		}
		break;
	}

	free(node);
}

/* Parse a stream of tokens into nodes. Consumes the tokens. */
static struct layout_node *
layout_parse_layout_tokens(struct layout_tokens **tokens)
{
	struct layout_token	*toks = (*tokens)->toks;
	struct layout_node	*layout;

	if (toks->type == TOK_OPENOBJECT)
		layout = layout_parse_object(&toks, NULL, NULL);
	else
		goto fail;

	if (layout == NULL)
		goto fail;

	if (toks->type != TOK_EOF) {
		layout_node_destroy(layout);
		layout = NULL;
	}
	layout_tokens_destroy(*tokens);
	*tokens = NULL;

	return (layout);
fail:
	layout_tokens_destroy(*tokens);
	*tokens = NULL;
	return (NULL);
}

/* Parse and return a key string, and advance the token pointer. */
static int
layout_parse_key(struct layout_token **toks, struct layout_string_view *lsv)
{
	if ((*toks)->type != TOK_QUOTE)
		return (-1);
	(*toks)++;
	if ((*toks)->type != TOK_VALUE)
		return (-1);

	lsv->ptr = (*toks)->val.ptr;
	lsv->len = (*toks)->val.len;

	(*toks)++;
	if ((*toks)->type != TOK_QUOTE)
		return (-1);

	(*toks)++;

	return (0);
}

/* Parse an object value, return the node, and advance the token pointer. */
static struct layout_node *
layout_parse_object(struct layout_token **toks, struct layout_node *parent,
    struct layout_string_view *key)
{
	struct layout_node		*object, *field;
	struct layout_string_view	 fkey;

	if ((*toks)->type != TOK_OPENOBJECT)
		return (NULL);
	(*toks)++;

	object = layout_node_create(parent, NODE_OBJECT, key, NULL);
	while ((*toks)->type != TOK_CLOSEOBJECT) {
		if (layout_parse_key(toks, &fkey) != 0)
			goto fail;
		if ((*toks)->type != TOK_COLON)
			goto fail;
		(*toks)++;

		switch ((*toks)->type) {
		case TOK_QUOTE:
			field = layout_parse_string(toks, object, &fkey);
			break;
		case TOK_VALUE:
			if (lsvcmp(&(*toks)->val, "true") == 0 ||
			    lsvcmp(&(*toks)->val, "false") == 0) {
				field = layout_parse_boolean(toks, object,
				    &fkey);
			} else {
				field = layout_parse_number(toks, object,
				    &fkey);
			}
			break;
		case TOK_OPENOBJECT:
			field = layout_parse_object(toks, object, &fkey);
			break;
		case TOK_OPENARRAY:
			field = layout_parse_array(toks, object, &fkey);
			break;
		default:
			goto fail;
		}
		if (field == NULL)
			goto fail;
		layout_node_assign(object, field);

		if ((*toks)->type == TOK_COMMA) {
			if ((*toks)[1].type == TOK_CLOSEOBJECT)
				goto fail;
			(*toks)++;
		} else if ((*toks)->type != TOK_CLOSEOBJECT)
			goto fail;
	}
	(*toks)++;
	return (object);
fail:
	layout_node_destroy(object);
	return (NULL);
}

/* Parse an array value, return the node, and advance the token pointer. */
static struct layout_node *
layout_parse_array(struct layout_token **toks, struct layout_node *parent,
    struct layout_string_view *key)
{
	struct layout_node	*array;
	struct layout_node	*member;

	if ((*toks)->type != TOK_OPENARRAY)
		return (NULL);
	(*toks)++;

	array = layout_node_create(parent, NODE_ARRAY, key, NULL);
	while ((*toks)->type != TOK_CLOSEARRAY) {
		switch ((*toks)->type) {
		case TOK_OPENOBJECT:
			member = layout_parse_object(toks, array, NULL);
			break;
		case TOK_COMMA:
			if ((*toks)[1].type != TOK_OPENOBJECT ||
			    (*toks)[-1].type != TOK_CLOSEOBJECT)
				goto fail;
			(*toks)++;
			continue;
		default:
			goto fail;
		}
		if (member == NULL)
			goto fail;
		layout_node_assign(array, member);

		if ((*toks)->type != TOK_COMMA &&
		    (*toks)->type != TOK_CLOSEARRAY)
			goto fail;
	}
	(*toks)++;
	return (array);
fail:
	layout_node_destroy(array);
	return (NULL);
}

/* Parse a string value, return the node, and advance the token pointer. */
static struct layout_node *
layout_parse_string(struct layout_token **toks, struct layout_node *parent,
    struct layout_string_view *key)
{
	struct layout_string_view	*val;

	if ((*toks)->type != TOK_QUOTE)
		return (NULL);
	(*toks)++;
	if ((*toks)->type != TOK_VALUE)
		return (NULL);

	val = &(*toks)->val;
	(*toks)++;

	if ((*toks)->type != TOK_QUOTE)
		return (NULL);
	(*toks)++;

	return (layout_node_create(parent, NODE_STRING, key, val));
}

/* Parse a number value, return the node, and advance the token pointer. */
static struct layout_node *
layout_parse_number(struct layout_token **toks, struct layout_node *parent,
    struct layout_string_view *key)
{
	const char	*numstr = (*toks)->val.ptr;
	char		*endptr;
	int64_t		 val;

	errno = 0;
	val = strtoll(numstr, &endptr, 10);
	if (errno != 0 || endptr != numstr + (*toks)->val.len)
		return (NULL);
	(*toks)++;

	return (layout_node_create(parent, NODE_NUMBER, key, &val));
}

/* Parse a boolean value, return the node, and advance the token pointer. */
static struct layout_node *
layout_parse_boolean(struct layout_token **toks, struct layout_node *parent,
    struct layout_string_view *key)
{
	int		val;

	if (lsvcmp(&(*toks)->val, "true") == 0)
		val = 1;
	else if (lsvcmp(&(*toks)->val, "false") == 0)
		val = 0;
	else
		return (NULL);

	(*toks)++;

	return (layout_node_create(parent, NODE_BOOLEAN, key, &val));
}

/* Return 1 when the node's key is equal to the parameter. */
static int
layout_key_is_eq(const struct layout_node *field, const char *key)
{
	return (lsvcmp(&field->key, key) == 0);
}

/* Return 1 when the node's value is equal to the parameter. */
static int
layout_val_is_eq(const struct layout_node *field, const void *val)
{
	switch (field->type) {
	case NODE_STRING:
		return (lsvcmp(&field->val.lsv, val) == 0);
	case NODE_NUMBER:
		return (field->val.num == *(int64_t *)val);
	case NODE_BOOLEAN:
		return (field->val.bool == *(int *)val);
	case NODE_OBJECT:
	case NODE_ARRAY:
		fatalx("unknown value comparison");
	}
	return (0);
}

/*
 * Evaluate parsed nodes. Check metadata at the top level and return the new
 * layout root. Consumes the layout_node tree.
 */
static struct layout_cell *
layout_evaluate_nodes(struct layout_node *root, int version)
{
	struct layout_node	*field;
	struct layout_cell	*lcroot = NULL;
	int			 ver = -1;

	TAILQ_FOREACH(field, &root->val.fields, entry) {
		switch (field->type) {
		case NODE_NUMBER:
			if (layout_key_is_eq(field, "V"))
				ver = field->val.num;
			break;
		case NODE_OBJECT:
			if (layout_key_is_eq(field, "L")) {
				if (lcroot != NULL)
					goto fail;
				lcroot = layout_evaluate_layout(field, NULL);
				if (lcroot == NULL)
					goto fail;
			}
			break;
		default:
			break;
		}
	}
	if (ver != version)
		goto fail;
	if (lcroot == NULL)
		goto fail;
	layout_node_destroy(root);

	return (lcroot);
fail:
	layout_node_destroy(root);
	if (lcroot != NULL)
		layout_free_cell(lcroot, 0);
	return (NULL);
}

/* Evaluate nodes into layout cells. */
static struct layout_cell *
layout_evaluate_layout(struct layout_node *node, struct layout_cell *lcparent)
{
	struct layout_node	*field, *fieldc;
	struct layout_cell	*lc = layout_create_cell(lcparent), *lcchild;
	enum layout_type	 type = LAYOUT_WINDOWPANE;
	const char		*numstr;
	u_int			 sx = UINT_MAX, sy = UINT_MAX;
	int			 xoff = INT_MAX, yoff = INT_MAX;
	__unused int		 id, last, active, zindex;

	TAILQ_FOREACH(field, &node->val.fields, entry) {
		switch (field->type) {
		case NODE_NUMBER:
			if (layout_key_is_eq(field, "w"))
				sx = field->val.num;
			else if (layout_key_is_eq(field, "h"))
				sy = field->val.num;
			else if (layout_key_is_eq(field, "x"))
				xoff = field->val.num;
			else if (layout_key_is_eq(field, "y"))
				yoff = field->val.num;
			else if (layout_key_is_eq(field, "l"))
				last = field->val.num; /* unused */
			else if (layout_key_is_eq(field, "z")) {
				zindex = field->val.num; /* unused */
				lc->flags |= LAYOUT_CELL_FLOATING;
			}
			break;
		case NODE_BOOLEAN:
			if (layout_key_is_eq(field, "a"))
				active = field->val.bool; /* unused */
			break;
		case NODE_STRING:
			if (layout_key_is_eq(field, "t")) {
				if (layout_val_is_eq(field, "h"))
					type = LAYOUT_LEFTRIGHT;
				else if (layout_val_is_eq(field, "v"))
					type = LAYOUT_TOPBOTTOM;
				else if (layout_val_is_eq(field, "p"))
					type = LAYOUT_WINDOWPANE;
				else
					goto fail;
				lc->type = type;
			} else if (layout_key_is_eq(field, "i")) {
				errno = 0;
				numstr = field->val.lsv.ptr;
				if (*numstr != '%')
					goto fail;
				id = strtol(numstr + 1, NULL, 10);
				if (errno != 0)
					goto fail;
			}
			break;
		case NODE_ARRAY:
			if (!layout_key_is_eq(field, "c"))
				break;
			TAILQ_FOREACH(fieldc, &field->val.fields, entry) {
				lcchild = layout_evaluate_layout(fieldc, lc);
				if (lcchild == NULL)
					goto fail;
				TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
			}
			break;
		default:
			break;
		}
	}
	if (type == LAYOUT_WINDOWPANE && !TAILQ_EMPTY(&lc->cells))
		goto fail;
	if (type != LAYOUT_WINDOWPANE && TAILQ_EMPTY(&lc->cells))
		goto fail;

	if (sx == UINT_MAX || sy == UINT_MAX || xoff == INT_MAX ||
	    yoff == INT_MAX)
		goto fail;
	layout_set_size(lc, sx, sy, xoff, yoff);
	return (lc);
fail:
	/*
	 * Ensure the children are freed if the child field is parsed and fails
	 * before the layout type is set.
	 */
	if (type == LAYOUT_WINDOWPANE && !TAILQ_EMPTY(&lc->cells))
		lc->type = LAYOUT_TOPBOTTOM;

	layout_free_cell(lc, 0);
	return (NULL);
}

/* Initialize a layout string. */
static void
layout_string_init(struct layout_string *ls)
{
	ls->dat[0] = '\0';
	ls->write = ls->dat;
}

/* Write an optionally formatted string to the end of the layout string. */
static int
layout_string_write(struct layout_string *ls, const char *fmt, ...)
{
	int	len, remaining = sizeof ls->dat - (ls->write - ls->dat);
	va_list ap;

	va_start(ap, fmt);

	len = vsnprintf(ls->write, remaining, fmt, ap);
	va_end(ap);

	if (len < 0 || len >= remaining)
		return (-1);
	ls->write += len;

	return (0);
}

/* Find the bottom-right cell. */
static struct layout_cell *
layout_find_bottomright(struct layout_cell *lc)
{
	if (lc->type == LAYOUT_WINDOWPANE)
		return (lc);
	lc = TAILQ_LAST(&lc->cells, layout_cells);
	return (layout_find_bottomright(lc));
}

/* Calculate layout checksum. */
static u_short
layout_checksum(const char *layout)
{
	u_short	csum;

	csum = 0;
	for (; *layout != '\0'; layout++) {
		csum = (csum >> 1) + ((csum & 1) << 15);
		csum += *layout;
	}
	return (csum);
}

/* Dump layout as a string. */
char *
layout_dump(__unused struct window *w, struct layout_cell *root, int flags)
{
	struct layout_string	 layout;
	char			*out;

	layout_string_init(&layout);

	if (layout_append(root, &layout, flags) != 0)
		return (NULL);

	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		xasprintf(&out, "%04hx,%s", layout_checksum(layout.dat),
		    layout.dat);
	else
		xasprintf(&out, "{\"V\":2,\"L\":%s}", layout.dat);
	return (out);
}

/* Append information for a single cell in a JSON (v2) format. */
static int
layout_append_v2(struct layout_cell *lc, struct layout_string *ls)
{
	struct layout_cell	*lcchild;
	struct window_pane	*wp;
	enum layout_type	 type;
	char			 c;
	u_int			 i, n;

	if (lc == NULL)
		return (0);

	type = lc->type;
	if (type == LAYOUT_TOPBOTTOM)
		c = 'v';
	else if (type == LAYOUT_LEFTRIGHT)
		c = 'h';
	else if (type == LAYOUT_WINDOWPANE)
		c = 'p';
	else
		return (-1);

	if (layout_string_write(ls, "{\"t\":\"%c\",\"w\":%u,\"h\":%u,\"x\":%d"
	    ",\"y\":%d", c, lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff) != 0)
		return (-1);
	if (type != LAYOUT_WINDOWPANE) {
		if (layout_string_write(ls, ",\"c\":[") != 0)
			return (-1);
		n = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append_v2(lcchild, ls) != 0)
				return (-1);
			if (layout_string_write(ls, ",") != 0)
				return (-1);
			n++;
		}
		if (n == 0)
			return (-1);
		*(--ls->write) = '\0'; /* removing trailing comma */
		if (layout_string_write(ls, "]") != 0)
			return (-1);
	} else {
		wp = lc->wp;
		if (wp == NULL)
			return (-1);
		if (wp == wp->window->active) {
			if (layout_string_write(ls, ",\"a\":true") != 0)
				return (-1);
		} else {
			if (window_pane_last_index(wp, &i) != -1) {
				if (layout_string_write(ls, ",\"l\":%u", i)
				    != 0)
					return (-1);
			}
		}
		if (lc->flags & LAYOUT_CELL_FLOATING) {
			if (window_pane_zindex(wp, &i) != -1) {
				if (layout_string_write(ls, ",\"z\":%u", i)
				    != 0)
					return (-1);
			}
		}
		if (layout_string_write(ls, ",\"i\":\"%%%u\"", wp->id) != 0)
			return (-1);
	}

	if (layout_string_write(ls, "}") != 0)
		return (-1);

	return (0);
}

/* Append information for a single cell in the legacy (v1) format. */
static int
layout_append_v1(struct layout_cell *lc, struct layout_string *ls)
{
	struct layout_cell	*lcchild;
	const char		*brackets = "[]";
	int			 n;

	if (lc == NULL)
		return (0);

	if (lc->wp != NULL) {
		if (layout_string_write(ls, "%ux%u,%d,%d,%u", lc->g.sx,
		    lc->g.sy, lc->g.xoff, lc->g.yoff, lc->wp->id) != 0)
			return (-1);
	} else {
		if (layout_string_write(ls, "%ux%u,%d,%d", lc->g.sx,
		    lc->g.sy, lc->g.xoff, lc->g.yoff) != 0)
			return (-1);
	}
	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		brackets = "{}";
		/* FALLTHROUGH */
	case LAYOUT_TOPBOTTOM:
		if (layout_string_write(ls, "%c", brackets[0]) != 0)
			return (-1);
		n = 0;
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (layout_append_v1(lcchild, ls) != 0)
				return (-1);
			if (layout_string_write(ls, ",") != 0)
				return (-1);
			n++;
		}
		if (n == 0)
			return (-1);

		*(--ls->write) = '\0'; /* removing trailing comma */
		if (layout_string_write(ls, "%c", brackets[1]) != 0)
			return (-1);
		break;
	case LAYOUT_WINDOWPANE:
		break;
	}

	return (0);
}

/* Dispatch to append the appropriate version. */
static int
layout_append(struct layout_cell *lc, struct layout_string *ls, int flags)
{
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		return (layout_append_v1(lc, ls));
	return (layout_append_v2(lc, ls));
}

/* Check layout sizes fit. */
static int
layout_check(struct layout_cell *lc)
{
	struct layout_cell	*lcchild;
	u_int			 n = 0;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		break;
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (lcchild->g.sy != lc->g.sy)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sx + 1;
		}
		if (n != 0 && n - 1 != lc->g.sx)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (!layout_cell_is_tiled(lcchild) &&
			    !layout_cell_has_tiled_child(lcchild))
				continue;
			if (lcchild->g.sx != lc->g.sx)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sy + 1;
		}
		if (n != 0 && n - 1 != lc->g.sy)
			return (0);
		break;
	}
	return (1);
}

/* Parse a layout string and arrange window as layout. */
int
layout_parse(struct window *w, const char *layout, char **cause)
{
	struct layout_cell	*lcchild, *lc = NULL;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx = 0, sy = 0;

	/* Build the layout. */
	lc = layout_construct(layout, cause);
	if (lc == NULL)
		return (-1);

	/* Check this window will fit into the layout. */
	npanes = window_count_panes(w, 1);
	for (;;) {
		ncells = layout_count_cells(lc);
		if (npanes > ncells) {
			xasprintf(cause, "have %u panes but need %u", npanes,
			    ncells);
			goto fail;
		}
		if (npanes == ncells)
			break;

		/*
		 * Fewer panes than cells, close the bottom right until none
		 * remain.
		 */
		lcchild = layout_find_bottomright(lc);
		layout_destroy_cell(w, lcchild, &lc);
	}

	/*
	 * It appears older versions of tmux were able to generate layouts with
	 * an incorrect top cell size - if it is larger than the top child then
	 * correct that (if this is still wrong the check code will catch it).
	 */
	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		break;
	case LAYOUT_LEFTRIGHT:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_cell_is_tiled(lcchild) ||
			    layout_cell_has_tiled_child(lcchild)) {
				sy = lcchild->g.sy + 1;
				sx += lcchild->g.sx + 1;
			}
		}
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_cell_is_tiled(lcchild) ||
			    layout_cell_has_tiled_child(lcchild)) {
				sx = lcchild->g.sx + 1;
				sy += lcchild->g.sy + 1;
			}
		}
		break;
	}
	if (lc->type != LAYOUT_WINDOWPANE && sx != 0 && sy != 0 &&
	    (lc->g.sx != sx || lc->g.sy != sy)) {
		layout_print_cell(lc, __func__, 0);
		lc->g.sx = sx - 1; lc->g.sy = sy - 1;
	}

	/* Check the new layout. */
	if (!layout_check(lc)) {
		*cause = xstrdup("size mismatch after applying layout");
		goto fail;
	}

	/* Resize window to the layout size. */
	if (layout_cell_is_tiled(lc) ||
	    layout_cell_has_tiled_child(lc))
		window_resize(w, lc->g.sx, lc->g.sy, -1, -1);

	/* Destroy the old layout and swap to the new. */
	layout_free_cell(w->layout_root, 0);
	w->layout_root = lc;

	/* Assign the panes into the cells. */
	wp = TAILQ_FIRST(&w->panes);
	layout_assign(&wp, lc);

	/* Update pane offsets and sizes. */
	layout_fix_zindexes(w);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	recalculate_sizes();
	layout_print_cell(lc, __func__, 0);

	events_fire_window("window-layout-changed", w);

	return (0);

fail:
	layout_free_cell(lc, 0);
	return (-1);
}

/* Assign panes into cells. */
static void
layout_assign(struct window_pane **wp, struct layout_cell *lc)
{
	struct layout_cell	*lcchild;

	if (lc == NULL)
		return;

	switch (lc->type) {
	case LAYOUT_WINDOWPANE:
		layout_make_leaf(lc, *wp);
		*wp = TAILQ_NEXT(*wp, entry);
		return;
	case LAYOUT_LEFTRIGHT:
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry)
			layout_assign(wp, lcchild);
		return;
	}
}

/* Construct a cell from the legacy (v1) format. */
static struct layout_cell *
layout_construct_cell(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell     *lc;
	u_int			sx, sy;
	int			xoff, yoff;
	const char	       *saved;

	if (!isdigit((u_char) **layout))
		return (NULL);
	if (sscanf(*layout, "%ux%u,%d,%d", &sx, &sy, &xoff, &yoff) != 4)
		return (NULL);

	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != 'x')
		return (NULL);
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != ',')
		return (NULL);
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout != ',')
		return (NULL);
	(*layout)++;
	while (isdigit((u_char) **layout))
		(*layout)++;
	if (**layout == ',') {
		saved = *layout;
		(*layout)++;
		while (isdigit((u_char) **layout))
			(*layout)++;
		if (**layout == 'x')
			*layout = saved;
	}

	lc = layout_create_cell(lcparent);
	lc->g.sx = sx;
	lc->g.sy = sy;
	lc->g.xoff = xoff;
	lc->g.yoff = yoff;

	return (lc);
}

/* Construct a layout from the legacy (v1) format. */
static struct layout_cell *
layout_construct_v1(struct layout_cell *lcparent, const char **layout)
{
	struct layout_cell	*lc, *lcchild;

	lc = layout_construct_cell(lcparent, layout);
	if (lc == NULL)
		return (NULL);

	switch (**layout) {
	case ',':
	case '}':
	case ']':
	case '\0':
		return (lc);
	case '{':
		(lc)->type = LAYOUT_LEFTRIGHT;
		break;
	case '[':
		(lc)->type = LAYOUT_TOPBOTTOM;
		break;
	default:
		goto fail;
	}

	do {
		(*layout)++;
		lcchild = layout_construct_v1(lc, layout);
		if (lcchild == NULL)
			goto fail;
		TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
	} while (**layout == ',');

	switch (lc->type) {
	case LAYOUT_LEFTRIGHT:
		if (**layout != '}')
			goto fail;
		break;
	case LAYOUT_TOPBOTTOM:
		if (**layout != ']')
			goto fail;
		break;
	default:
		goto fail;
	}
	(*layout)++;

	return (lc);

fail:
	layout_free_cell(lc, 0);
	return (NULL);
}

/* Construct a layout root from a formatted string. */
static struct layout_cell *
layout_construct(const char *layout, char **cause)
{
	struct layout_cell	*lc;
	struct layout_tokens	*tokens = NULL;
	struct layout_node	*node = NULL;
	u_short			 csum;
	int			 n;

	while (isspace((u_char) *layout))
		layout++;

	if (*layout != '{') { /* sniffing version */
		if (sscanf(layout, "%hx,%n", &csum, &n) != 1 || n != 5) {
			*cause = xstrdup("malformed layout header");
			return (NULL);
		}
		layout += n;
		if (csum != layout_checksum(layout)) {
			*cause = xstrdup("invalid layout checksum");
			return (NULL);
		}
		lc = layout_construct_v1(NULL, &layout);
	} else {
		if ((tokens = layout_tokenize_input(layout)) == NULL) {
			*cause = xstrdup("invalid layout characters");
			return (NULL);
		}
		if ((node = layout_parse_layout_tokens(&tokens)) == NULL) {
			*cause = xstrdup("invalid layout json");
			return (NULL);
		}
		lc = layout_evaluate_nodes(node, 2);
	}
	if (lc == NULL)
		*cause = xstrdup("invalid layout");

	return (lc);
}
