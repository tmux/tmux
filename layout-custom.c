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
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Layouts can be represented as strings and currently in two formats: a JSON
 * format (v2, *current*), and a legacy format (v1). The legacy format will be
 * deprecated at some point in the future. Please work off of the current
 * format.
 *
 * The v2 format is of the form {"V":<version>,"L":<layout cells>}. The layout
 * cells have fields "w", "h", "x", and "y". Additionally, leaf cells have "i",
 * "l" (if not active), "a" (if active), "z" (if floating), and node cells have
 * "c". See below for more details.
 *
 * The v1 format starts with a 4 digit checksum, and then lists cells by their
 * dimensions of the form '<width>x<height>,<xoffset>,<yoffset>'. Nodes are
 * followed by brackets which hold the children of the node, with '{}' for
 * vertical splits, and '[]' for horizontal splits. The checksum and cells are
 * also separated by commas.
 */

/* Maximums */
#define LAYOUT_STRING_MAX	8192
#define TOKENS_MAX		(LAYOUT_STRING_MAX)

/* Recognized keys for JSON format. */
#define KEY_VERSION	"V"	/* number */
#define KEY_LAYOUT	"L"	/* object */
#define KEY_TYPE	"t"	/* string, see accepted vals below */
#define KEY_WIDTH	"w"	/* number, positive */
#define KEY_HEIGHT	"h"	/* number, positive */
#define KEY_XOFFSET	"x"	/* number */
#define KEY_YOFFSET	"y"	/* number */
#define KEY_PANEID	"i"	/* string, "%<num>" */
#define KEY_LASTPANE	"l"	/* number, position in w->lastpanes */
#define KEY_ACTIVE	"a"	/* boolean */
#define KEY_ZINDEX	"z"	/* number, only floats position in w->z_index */
#define KEY_CHILDREN	"c"	/* array, only nodes */

/* Recognized values for KEY_TYPE. */
#define VAL_TYPE_VERTICAL	"v"
#define VAL_TYPE_HORIZONTAL	"h"
#define VAL_TYPE_PANE		"p"

/* Token types. */
enum token_type {
	TOK_OPENCURLY,
	TOK_CLOSEDCURLY,
	TOK_OPENBRACKET,
	TOK_CLOSEDBRACKET,
	TOK_COMMA,
	TOK_COLON,
	TOK_QUOTE,
	TOK_VALUE,
	TOK_EOF
};

/* Token. */
struct token {
	enum token_type	 type;
	char		*val;
};

/* Tokens. */
struct tokens {
	struct token	*toks;
	int		 size;
};

/* Node type. */
enum node_type {
	NODE_STRING,
	NODE_NUMBER,
	NODE_BOOLEAN,
	NODE_OBJECT,
	NODE_ARRAY
};

/* Node queue. */
TAILQ_HEAD(nodes, node);

/* Node. */
struct node {
	struct node		*parent;
	const char		*key;
	enum node_type		 type;
	union {
		const char	*s;
		int		*i;
		int		*b;
	} val;
	struct nodes		 fields;
	TAILQ_ENTRY(node)	 entry;
};

/* Layout string. */
struct layout_string {
	char	*write;
	char	 dat[LAYOUT_STRING_MAX];
};


static struct tokens	*tokenize_layout_string(const char *);
static struct tokens	*tokens_create(void);
static void		 tokens_destroy(struct tokens *);
static int		 tokens_push(struct tokens *, enum token_type, char *);
static struct node	*node_create(struct node *, enum node_type ,
			     const char *, const void *);
static void		 node_destroy(struct node *);
static void		 node_assign(struct node *, const void *);
static struct node	*parse_layout(struct tokens **);
static const char	*parse_key(struct token **);
static struct node	*parse_object(struct token **, struct node *,
			     const char *);
static struct node	*parse_array(struct token **, struct node *,
			     const char *);
static struct node	*parse_string(struct token **, struct node *,
			     const char *);
static struct node	*parse_number(struct token **, struct node *,
			     const char *);
static struct node	*parse_boolean(struct token **, struct node *,
			     const char *);
static int		 key_is_eq(const struct node *, const char *);
static int		 val_is_eq(const struct node *, const void *);
static struct layout_cell *evaluate_nodes(struct node *, int);
static struct layout_cell *evaluate_layout(struct node *, struct layout_cell *);

static struct layout_cell	*layout_find_bottomright(struct layout_cell *);
static u_short			 layout_checksum(const char *);
static int			 layout_append(struct layout_cell *,
				     struct layout_string *, size_t, int);
static struct layout_cell	*layout_construct(const char *, int, char **);
static void			 layout_assign(struct window_pane **,
				     struct layout_cell *);

/* Tokenize the layout string. */
static struct tokens *
tokenize_layout_string(const char *input)
{
	struct tokens	*tokens = tokens_create();
	enum token_type	 type;
	char		*val = NULL;
	int		 scan;

	while (*input != '\0') {
		switch (*input) {
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				input++;
				continue;
			case '{':
				type = TOK_OPENCURLY;
				break;
			case '}':
				type = TOK_CLOSEDCURLY;
				break;
			case '[':
				type = TOK_OPENBRACKET;
				break;
			case ']':
				type = TOK_CLOSEDBRACKET;
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
				scan = 0;
				while (input[scan] != '"' && input[scan] != ','
				    && input[scan] != '\0')
					scan++;
				if (input[scan] == '\0')
					goto fail;
				if (tokens->size + scan > TOKENS_MAX)
					goto fail;
				val = xstrndup(input, scan);
				input += scan - 1;
		}
		if (tokens_push(tokens, type, val) != 0)
			goto fail;
		input++;
		val = NULL;
	}
	if (tokens_push(tokens, TOK_EOF, NULL) != 0)
		goto fail;

	return (tokens);
fail:
	tokens_destroy(tokens);
	return (NULL);
}

/* Create a new token container. */
static struct tokens *
tokens_create(void)
{
	struct tokens	*tokens;

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
tokens_destroy(struct tokens *tokens)
{
	int	i;

	for (i = 0; i < tokens->size; i++) {
		if (tokens->toks[i].val != NULL)
			free(tokens->toks[i].val);
	}
	free(tokens->toks);
	tokens->toks = NULL;
	free(tokens);
}

/* Add a token to the token container. */
static int
tokens_push(struct tokens *tokens, enum token_type type, char *val)
{
	struct token	*tok;

	if (tokens->size > TOKENS_MAX)
		return (-1);

	tok = &tokens->toks[tokens->size++];
	tok->type = type;
	tok->val = val;

	return (0);
}

/* Create a node and assign given values. */
static struct node *
node_create(struct node *parent, enum node_type type, const char *key,
    const void *val)
{
	struct node	*node;

	node = xcalloc(1, sizeof *node);
	node->parent = parent;
	node->key = key;
	node->type = type;
	TAILQ_INIT(&node->fields);
	if (val != NULL)
		node_assign(node, val);

	return (node);
}

/* Assign a value to a node. */
static void
node_assign(struct node *node, const void *val)
{
	struct node	*child = (struct node *)val;

	switch (node->type) {
		case NODE_STRING:
			node->val.s = (const char *)val;
			break;
		case NODE_NUMBER:
			node->val.i = (int *)val;
			break;
		case NODE_BOOLEAN:
			node->val.b = (int *)val;
			break;
		case NODE_OBJECT:
		case NODE_ARRAY:
			TAILQ_INSERT_TAIL(&node->fields, child, entry);
			break;
		default:
			fatalx("unknown node type");
	}
}

/* Destroy a node and all of the nodes fields. */
static void
node_destroy(struct node *node)
{
	struct node	*field;

	if (node == NULL)
		return;

	if (node->key != NULL)
		free((void *)node->key);

	switch (node->type) {
	case NODE_OBJECT:
	case NODE_ARRAY:
		field = TAILQ_FIRST(&node->fields);
		while (!TAILQ_EMPTY(&node->fields)) {
			TAILQ_REMOVE(&node->fields, field, entry);
			node_destroy(field);
			field = TAILQ_FIRST(&node->fields);
		}
		break;
	case NODE_STRING:
		free((char *)node->val.s);
		break;
	case NODE_NUMBER:
		free(node->val.i);
		break;
	case NODE_BOOLEAN:
		free(node->val.b);
		break;
	}

	free(node);
}

/* Parse a stream of tokens into nodes. Consumes the tokens. */
static struct node *
parse_layout(struct tokens **tokens)
{
	struct token	*toks = (*tokens)->toks;
	struct node	*layout;

	if (toks->type == TOK_OPENCURLY)
		layout = parse_object(&toks, NULL, NULL);
	else
		goto fail;

	if (layout == NULL)
		goto fail;

	if (toks->type != TOK_EOF) {
		node_destroy(layout);
		layout = NULL;
	}
	tokens_destroy(*tokens);
	*tokens = NULL;

	return (layout);
fail:
	tokens_destroy(*tokens);
	*tokens = NULL;
	return (NULL);
}

/* Parse and return a key string, and advance the token pointer. */
static const char *
parse_key(struct token **toks)
{
	const char	*key = NULL;

	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;
	if ((*toks)->type != TOK_VALUE)
		goto fail;

	key = (*toks)->val;
	(*toks)->val = NULL;

	(*toks)++;
	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;

	 return (key);
fail:
	if (key != NULL)
		free((void *)key);
	return (NULL);
}

/* Parse a object value, return the node, and advance the token pointer. */
static struct node *
parse_object(struct token **toks, struct node *parent, const char *key)
{
	struct node	*object = node_create(parent, NODE_OBJECT, key, NULL);
	struct node	*field;
	const char	*fkey = NULL;

	if ((*toks)->type != TOK_OPENCURLY)
		goto fail;
	(*toks)++;

	while ((*toks)->type != TOK_CLOSEDCURLY) {
		if ((fkey = parse_key(toks)) == NULL)
			goto fail;
		if ((*toks)->type != TOK_COLON)
			goto fail;
		(*toks)++;

		switch ((*toks)->type) {
		case TOK_QUOTE:
			if ((field = parse_string(toks, object, fkey)) == NULL)
				goto fail;
			break;
		case TOK_VALUE:
			if (isdigit(*(*toks)->val)) {
				if ((field = parse_number(toks, object, fkey))
				    == NULL)
					goto fail;
			} else {
				if ((field = parse_boolean(toks, object, fkey))
				    == NULL)
					goto fail;
			}
			break;
		case TOK_OPENCURLY:
			if ((field = parse_object(toks, object, fkey)) == NULL)
				goto fail;
			break;
		case TOK_OPENBRACKET:
			if ((field = parse_array(toks, object, fkey)) == NULL)
				goto fail;
			break;
		default:
			goto fail;
		}
		node_assign(object, field);

		if ((*toks)->type == TOK_COMMA) {
			if ((*toks)[1].type == TOK_CLOSEDCURLY)
				goto fail;
			(*toks)++;
		} else if ((*toks)->type != TOK_CLOSEDCURLY)
			goto fail;
	}
	(*toks)++;
	return (object);
fail:
	if (key != NULL)
		free((void *)key);
	node_destroy(object);
	return (NULL);
}

/* Parse a array value, return the node, and advance the token pointer. */
static struct node *
parse_array(struct token **toks, struct node *parent, const char *key)
{
	struct node	*array = node_create(parent, NODE_ARRAY, key, NULL);
	struct node	*member;

	if ((*toks)->type != TOK_OPENBRACKET)
		goto fail;
	(*toks)++;

	while ((*toks)->type != TOK_CLOSEDBRACKET) {
		switch ((*toks)->type) {
		case TOK_OPENCURLY:
			if ((member = parse_object(toks, array, NULL)) == NULL)
				goto fail;
			break;
		case TOK_COMMA:
			if ((*toks)[1].type != TOK_OPENCURLY ||
			    (*toks)[-1].type != TOK_CLOSEDCURLY)
				goto fail;
			(*toks)++;
			continue;
		default:
			goto fail;
		}
		node_assign(array, member);

		if ((*toks)->type != TOK_COMMA &&
		    (*toks)->type != TOK_CLOSEDBRACKET)
			goto fail;
	}
	(*toks)++;
	return (array);
fail:
	if (key != NULL)
		free((void *)key);
	node_destroy(array);
	return (NULL);
}

/* Parse a string value, return the node, and advance the token pointer. */
static struct node *
parse_string(struct token **toks, struct node *parent, const char *key)
{
	const char	*val = NULL;

	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;

	val = (*toks)->val;
	(*toks)->val = NULL;
	(*toks)++;

	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;

	return (node_create(parent, NODE_STRING, key, val));
fail:
	if (val != NULL)
		free((void *)val);
	return (NULL);
}

/* Parse a number value, return the node, and advance the token pointer. */
static struct node *
parse_number(struct token **toks, struct node *parent, const char *key)
{
	const char	*cause = NULL;
	int		*val = NULL;

	if (!isdigit(*(*toks)->val))
		goto fail;

	val = xmalloc(sizeof *val);
	*val = strtonum((*toks)->val, INT_MIN, INT_MAX, &cause);
	if (cause != NULL) {
		goto fail;
	}
	free((*toks)->val);
	(*toks)->val = NULL;
	(*toks)++;

	return (node_create(parent, NODE_NUMBER, key, val));
fail:
	if (cause != NULL)
		free((void *)cause);
	if (val != NULL)
		free((void *)val);
	return (NULL);
}

/* Parse a boolean value, return the node, and advance the token pointer. */
static struct node *
parse_boolean(struct token **toks, struct node *parent, const char *key)
{
	int		*val = NULL;

	val = xmalloc(sizeof *val);
	if (strcmp((*toks)->val, "true") == 0)
		*val = 1;
	else if (strcmp((*toks)->val, "false") == 0)
		*val = 0;
	else {
		goto fail;
	}
	free((*toks)->val);
	(*toks)->val = NULL;
	(*toks)++;

	return (node_create(parent, NODE_BOOLEAN, key, val));
fail:
	if (val != NULL)
		free(val);
	return (NULL);
}

/* Return 1 if the node's key is equal to the parameter. */
static int
key_is_eq(const struct node *field, const char *key)
{
	return (strcmp(field->key, key) == 0);
}

/* Return 1 if the node's vaue is equal to the parameter. */
static int
val_is_eq(const struct node *field, const void *val)
{
	switch (field->type) {
	case NODE_STRING:
		return (strcmp(field->val.s, val) == 0);
	case NODE_BOOLEAN:
		return (*field->val.b == *(int *)val);
	case NODE_NUMBER:
		return (*field->val.i == *(int *)val);
	default:
		fatalx("node has no value");
	}
	return (0);
}

/*
 * Evaluate parsed nodes. Check metadata at the top level and return the new
 * layout root. Consumes the node tree.
 */
static struct layout_cell *
evaluate_nodes(struct node *root, int version)
{
	struct node		*field;
	struct layout_cell	*lcroot = NULL;
	int			 ver = -1;

	TAILQ_FOREACH(field, &root->fields, entry) {
		switch (field->type) {
		case NODE_NUMBER:
			if (key_is_eq(field, KEY_VERSION))
				ver = *field->val.i;
			break;
		case NODE_OBJECT:
			if (key_is_eq(field, KEY_LAYOUT))
				lcroot = evaluate_layout(field, NULL);
			break;
		default:
			break;
		}
	}

	if (ver != version)
		goto fail;
	if (lcroot == NULL)
		goto fail;

	return (lcroot);
fail:
	if (lcroot != NULL)
		layout_free_cell(lcroot, 0);
	return (NULL);
}

/* Evaluate nodes into layout cells. */
static struct layout_cell *
evaluate_layout(struct node *node, struct layout_cell *lcparent)
{
	struct node		*field, *fieldc;
	struct layout_cell	*lc = layout_create_cell(lcparent), *lcchild;
	enum layout_type	 type;
	u_int			 sx, sy;
	int			 xoff, yoff;
	__unused int		 id, last, active, zindex;

	TAILQ_FOREACH(field, &node->fields, entry) {
		switch (field->type) {
		case NODE_NUMBER:
			if (key_is_eq(field, KEY_WIDTH))
				sx = *field->val.i;
			else if (key_is_eq(field, KEY_HEIGHT))
				sy = *field->val.i;
			else if (key_is_eq(field, KEY_XOFFSET))
				xoff = *field->val.i;
			else if (key_is_eq(field, KEY_YOFFSET))
				yoff = *field->val.i;
			else if (key_is_eq(field, KEY_LASTPANE))
				last = *field->val.i; /* unused */
			else if (key_is_eq(field, KEY_ZINDEX)) {
				zindex = *field->val.i; /* unused */
				lc->flags |= LAYOUT_CELL_FLOATING;
			}
			break;
		case NODE_BOOLEAN:
			if (key_is_eq(field, KEY_ACTIVE))
				active = *field->val.i; /* unused */
			break;
		case NODE_STRING:
			if (key_is_eq(field, KEY_TYPE)) {
				if (val_is_eq(field, VAL_TYPE_HORIZONTAL))
					type = LAYOUT_LEFTRIGHT;
				else if (val_is_eq(field, VAL_TYPE_VERTICAL))
					type = LAYOUT_TOPBOTTOM;
				else if (val_is_eq(field, VAL_TYPE_PANE))
					type = LAYOUT_WINDOWPANE;
				else
					goto fail;
			} else if (key_is_eq(field, KEY_PANEID))
				id = strtonum(field->val.s + 1, INT_MIN,
				    INT_MAX, NULL); /*unused */
			break;
		case NODE_ARRAY:
			if (!key_is_eq(field, KEY_CHILDREN))
				break;
			TAILQ_FOREACH(fieldc, &field->fields, entry) {
				lcchild = evaluate_layout(fieldc, lc);
				if (lcchild == NULL)
					goto fail;
				TAILQ_INSERT_TAIL(&lc->cells, lcchild, entry);
			}
			break;
		default:
			break;
		}
	}
	if (type == LAYOUT_WINDOWPANE && TAILQ_FIRST(&lc->cells) != NULL)
		goto fail;

	layout_set_size(lc, sx, sy, xoff, yoff);
	lc->type = type;
	return (lc);
fail:
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
	char	tmp[128];
	size_t	len, remaining = sizeof ls->dat - (ls->write - ls->dat);
	va_list ap;

	va_start(ap, fmt);

	len = xvsnprintf(tmp, sizeof tmp, fmt, ap);
	if (len > (sizeof (tmp)) - 1)
		goto fail;
	if (remaining < len + 1) /* null terminator */
		goto fail;
	memcpy(ls->write, tmp, len);
	ls->write += len;
	*ls->write = '\0';

	va_end(ap);
	return (0);
fail:
	va_end(ap);
	return (-1);
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
	struct layout_string	 layout = { 0 };
	char			*out;

	layout_string_init(&layout);

	if (layout_append(root, &layout, sizeof layout.dat, flags) != 0)
		return (NULL);

	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		xasprintf(&out, "%04hx,%s", layout_checksum(layout.dat), layout.dat);
	else
		xasprintf(&out, "{\"V\":2,\"L\":%s}", layout.dat);
	return (out);
}

/* Append information for a single cell in a JSON (v2) format. */
static int
layout_append_v2(struct layout_cell *lc, struct layout_string *ls, size_t len)
{
	struct layout_cell	*lcchild;
	struct window_pane	*wp;
	enum layout_type	 type = lc->type;
	char			 c;
	u_int			 i;

	if (len == 0)
		return (-1);
	if (lc == NULL)
		return (0);

	if (type == LAYOUT_TOPBOTTOM)
		c = 'v';
	else if (type == LAYOUT_LEFTRIGHT)
		c = 'h';
	else if (LAYOUT_WINDOWPANE)
		c = 'p';
	else
		return (-1);

	if (layout_string_write(ls, "{\"t\":\"%c\",\"w\":%u,\"h\":%u,\"x\":%d"
	    ",\"y\":%d", c, lc->g.sx, lc->g.sy, lc->g.xoff, lc->g.yoff) != 0)
		return (-1);
	if (type != LAYOUT_WINDOWPANE) {
		if (layout_string_write(ls, ",\"c\":[") != 0)
			return (-1);
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append_v2(lcchild, ls, len) != 0)
				return (-1);
			if (layout_string_write(ls, ",") != 0)
				return (-1);
		}
		*(--ls->write) = '\0'; /* removing trailing comma */
		if (layout_string_write(ls, "]") != 0)
			return (-1);
	} else {
		wp = lc->wp;
		if (wp == wp->window->active) {
			if (layout_string_write(ls, ",\"a\":true") != 0)
				return (-1);
		} else {
			window_pane_last_index(wp, &i);
			if (layout_string_write(ls, ",\"l\":%u", i) != 0)
				return (-1);
		}
		if (lc->flags & LAYOUT_CELL_FLOATING) {
			window_pane_zindex(wp, &i);
			if (layout_string_write(ls, ",\"z\":%u", i) != 0)
				return (-1);
		}
		if (layout_string_write(ls, ",\"i\":\"%%%d\"", wp->id) != 0)
			return (-1);
	}

	if (layout_string_write(ls, "}") != 0)
		return (-1);

	return (0);
}

/* Append information for a single cell in the legacy (v1) format. */
static int
layout_append_v1(struct layout_cell *lc, struct layout_string *ls, size_t len)
{
	struct layout_cell     *lcchild;
	const char	       *brackets = "[]";

	if (len == 0)
		return (-1);
	if (lc == NULL || lc->flags & LAYOUT_CELL_FLOATING)
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
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (layout_append_v1(lcchild, ls, len) != 0)
				return (-1);
			if (layout_string_write(ls, ",") != 0)
				return (-1);
		}
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
layout_append(struct layout_cell *lc, struct layout_string *ls, size_t len,
    int flags)
{
	if (flags & LAYOUT_CUSTOM_OLD_FORMAT)
		return (layout_append_v1(lc, ls, len));
	return (layout_append_v2(lc, ls, len));
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
			if (lcchild->flags & LAYOUT_CELL_FLOATING)
				continue;
			if (lcchild->g.sy != lc->g.sy)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sx + 1;
		}
		if (n - 1 != lc->g.sx)
			return (0);
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (lcchild->flags & LAYOUT_CELL_FLOATING)
				continue;
			if (lcchild->g.sx != lc->g.sx)
				return (0);
			if (!layout_check(lcchild))
				return (0);
			n += lcchild->g.sy + 1;
		}
		if (n - 1 != lc->g.sy)
			return (0);
		break;
	}
	return (1);
}

/* Parse a layout string and arrange window as layout. */
int
layout_parse(struct window *w, const char *layout, int flags, char **cause)
{
	struct layout_cell	*lcchild, *lc = NULL;
	struct window_pane	*wp;
	u_int			 npanes, ncells, sx = 0, sy = 0;

	/* Build the layout. */
	lc = layout_construct(layout, flags, cause);
	if (lc == NULL) {
		return (-1);
	}
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
			if (~lcchild->flags & LAYOUT_CELL_FLOATING) {
				sy = lcchild->g.sy + 1;
				sx += lcchild->g.sx + 1;
			}
		}
		break;
	case LAYOUT_TOPBOTTOM:
		TAILQ_FOREACH(lcchild, &lc->cells, entry) {
			if (~lcchild->flags & LAYOUT_CELL_FLOATING) {
				sx = lcchild->g.sx + 1;
				sy += lcchild->g.sy + 1;
			}
		}
		break;
	}
	if (lc->type != LAYOUT_WINDOWPANE &&
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
	if (sx != 0 && sy != 0)
		window_resize(w, lc->g.sx, lc->g.sy, -1, -1);

	/* Destroy the old layout and swap to the new. */
	layout_free_cell(w->layout_root, 0);
	w->layout_root = lc;

	/* Assign the panes into the cells. */
	wp = TAILQ_FIRST(&w->panes);
	if (lc != NULL)
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

/* Construct a layout root from a formated string. */
static struct layout_cell *
layout_construct(const char *layout, int flags, char **cause)
{
	struct layout_cell	*lc;
	struct tokens		*tokens = NULL;
	struct node		*node = NULL;
	u_short			 csum;
	int			 n;

	if (flags & LAYOUT_CUSTOM_OLD_FORMAT) {
		if (sscanf(layout, "%hx,%n", &csum, &n) != 1 || n != 5) {
			*cause = xstrdup("malformed layout header");
			return (NULL);
		}
		layout += n;
		if (flags & LAYOUT_CUSTOM_OLD_FORMAT) {
			if (csum != layout_checksum(layout)) {
				*cause = xstrdup("invalid layout checksum");
				return (NULL);
			}
		}
		lc = layout_construct_v1(NULL, &layout);
	} else {
		;
		if ((tokens = tokenize_layout_string(layout)) == NULL) {
			*cause = xstrdup("invalid layout characters");
			return (NULL);
		}
		if ((node = parse_layout(&tokens)) == NULL) {
			*cause = xstrdup("invalid layout json");
			return (NULL);
		}
		lc = evaluate_nodes(node, 2);
	}
	if (lc == NULL)
		*cause = xstrdup("invalid layout");

	return (lc);
}
