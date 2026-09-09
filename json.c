/* $OpenBSD: json.c,v 1.1 2026/09/08 08:33:10 nicm Exp $ */

/*
 * Copyright (c) 2026 Dane Jensen <dhcjensen@gmail.com>
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Parse a subset of JSON. The subset accepted is:
 *
 * - Arrays may only hold objects.
 * - The top-level value must be an object.
 * - Numbers are 64-bit signed integers in base 10; there are no fractions and
 *   no exponents.
 * - There is no null, and a string may not be empty.
 * - Escapes are validated but not decoded.
 * - A key may not appear twice in the same object. Note that because escapes
 *   are not decoded, duplicate keys may go undetected.
 * - Objects may only be parsed to a fixed maximum depth.
 */

#define ERROR_CTX_LEN	8
#define PARSE_DEPTH_MAX	200

/* JSON token types. */
enum json_token_type {
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

/* JSON token. */
struct json_token {
	enum json_token_type	type;
	int			offset;
	int			len;
};

/* JSON tokens. */
struct json_tokens {
	int			 size;
	int			 capacity;
	struct json_token	*toks;
};

/* JSON node type. */
enum json_node_type {
	NODE_STRING,
	NODE_NUMBER,
	NODE_BOOLEAN,
	NODE_OBJECT,
	NODE_ARRAY
};

/* JSON field tree. */
RB_HEAD(json_fields, json_node);

/* JSON array queue. */
TAILQ_HEAD(json_members, json_node);

/* JSON parse context. */
struct json_parse_ctx {
	const char	 *input;
	char		**cause;
	int		  depth;
};

/* JSON node. */
struct json_node {
	enum json_node_type		 type;
	char				*key;
	struct json_node		*parent;
	union {
		char			*str;
		int64_t			 num;
		int			 boolean;
		struct json_fields	 fields;
		struct json_members	 members;
	};
	RB_ENTRY(json_node)		 oentry;
	TAILQ_ENTRY(json_node)		 aentry;
};

static int
json_node_cmp(struct json_node *a, struct json_node *b)
{
	return (strcmp(a->key, b->key));
}
RB_GENERATE_STATIC(json_fields, json_node, oentry, json_node_cmp);

static struct json_tokens *json_tokenize_input(const char *, char **);
static struct json_tokens *json_create_tokens(void);
static void		 json_destroy_tokens(struct json_tokens *);
static void		 json_add_token(struct json_tokens *,
			     enum json_token_type, const char *, const char *,
			     int);
static int		 json_tokenize_value(struct json_tokens *,
			     const char *);
static void		 json_error(char **, const char *, const char *);
static struct json_node	*json_create_node(struct json_node *,
			     enum json_node_type, const char *, void *);
static void		 json_assign_value(struct json_node *, void *);
static struct json_node *json_parse_tokens(struct json_tokens **,
			     struct json_parse_ctx *);
static char		*json_parse_key(struct json_token **,
			     struct json_parse_ctx *);
static struct json_node	*json_parse_object(struct json_token **,
			     struct json_parse_ctx *, const char *,
			     struct json_node *);
static struct json_node	*json_parse_array(struct json_token **,
			     struct json_parse_ctx *, const char *,
			     struct json_node *);
static struct json_node	*json_parse_string(struct json_token **,
			     struct json_parse_ctx *, const char *,
			     struct json_node *);
static struct json_node	*json_parse_number(struct json_token **,
			     struct json_parse_ctx *, const char *,
			     struct json_node *);
static struct json_node	*json_parse_boolean(struct json_token **,
			     struct json_parse_ctx *, const char *,
			     struct json_node *);

/* Parse an input string into JSON. */
struct json_node *
json_parse(const char *input, char **cause)
{
	struct json_tokens	*tokens;
	struct json_parse_ctx	 pctx;

	if (*input == '\0') {
		json_error(cause, "empty input", NULL);
		return (NULL);
	}

	if ((tokens = json_tokenize_input(input, cause)) == NULL)
		return (NULL);

	pctx.input = input;
	pctx.cause = cause;
	pctx.depth = 0;

	return (json_parse_tokens(&tokens, &pctx));
}

/* Returns a field node from an object node. */
struct json_node *
json_find(struct json_node *jn, const char *key)
{
	struct json_node	*node = (struct json_node *)jn, tmp = { 0 };

	if (jn->type != NODE_OBJECT)
		return (NULL);

	tmp.key = (char *)key;
	return (RB_FIND(json_fields, &node->fields, &tmp));
}

/* Returns the first member of an array node. */
struct json_node *
json_array_first(struct json_node *jn)
{
	if (jn->type != NODE_ARRAY)
		return (NULL);

	return (TAILQ_FIRST(&jn->members));
}

/* Returns the next member of an array's member node. */
struct json_node *
json_array_next(struct json_node *member)
{
	if (member == NULL ||
	    member->parent == NULL ||
	    member->parent->type != NODE_ARRAY)
		return (NULL);
	return (TAILQ_NEXT(member, aentry));
}

/* Returns the string value from a node. */
int
json_get_string(struct json_node *jn, const char **s)
{
	if (jn->type != NODE_STRING)
		return (-1);

	*s = jn->str;
	return (0);
}

/* Returns the number value from a node. */
int
json_get_number(struct json_node *jn, int64_t *i)
{
	if (jn->type != NODE_NUMBER)
		return (-1);

	*i = jn->num;
	return (0);
}

/* Returns the boolean value from a node. */
int
json_get_boolean(struct json_node *jn, int *b)
{
	if (jn->type != NODE_BOOLEAN)
		return (-1);

	*b = jn->boolean;
	return (0);
}

/* Returns the object value from a node. */
int
json_get_object(struct json_node *jn, struct json_node **o)
{
	if (jn->type != NODE_OBJECT)
		return (-1);

	*o = jn;
	return (0);
}

/* Returns the array value from a node. */
int
json_get_array(struct json_node *jn, struct json_node **a)
{
	if (jn->type != NODE_ARRAY)
		return (-1);

	*a = jn;
	return (0);
}

/* Returns the string value from a given key in an object node. */
int
json_find_string(struct json_node *jn, const char *key, const char **out,
    char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_STRING) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected a string", key);
		return (-1);
	}
	*out = field->str;

	return (0);
}

/* Returns the number value from a given key in an object node. */
int
json_find_number(struct json_node *jn, const char *key, int64_t *out,
    char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_NUMBER) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected a number", key);
		return (-1);
	}
	*out = field->num;

	return (0);
}

/* Returns the boolean value from a given key in an object node. */
int
json_find_boolean(struct json_node *jn, const char *key, int *out, char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_BOOLEAN) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected a boolean", key);
		return (-1);
	}
	*out = field->boolean;

	return (0);
}

/* Returns the object value from a given key in an object node. */
int
json_find_object(struct json_node *jn, const char *key, struct json_node **out,
    char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_OBJECT) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected an object", key);
		return (-1);
	}
	*out = field;

	return (0);
}

/* Returns the array value from a given key in an object node. */
int
json_find_array(struct json_node *jn, const char *key, struct json_node **out,
    char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_ARRAY) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected an array", key);
		return (-1);
	}
	*out = field;

	return (0);
}

/* Fill an error cause. */
static void
json_error(char **cause, const char *reason, const char *loc)
{
	const char	*ellipsis = "...";
	int		 i;

	if (cause == NULL)
		return;
	if (loc == NULL || *loc == '\0') {
		xasprintf(cause, "%s", reason);
		return;
	}

	for (i = 0; i < ERROR_CTX_LEN + 1; i++) {
		if (loc[i] == '\0') {
			ellipsis = "";
			break;
		}
	}

	xasprintf(cause, "%s: %.*s%s", reason, ERROR_CTX_LEN, loc, ellipsis);
}

/* Tokenize the json string. */
static struct json_tokens *
json_tokenize_input(const char *input, char **cause)
{
	struct json_tokens	*tokens;
	enum json_token_type	 type;
	const char		*loc, *start = input;
	int			 in_string = 0, scan;

	tokens = json_create_tokens();
	while (*input != '\0') {
		loc = input;
		scan = 1;

		if (in_string && *input != '"') {
			type = TOK_VALUE;
		} else {
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
				break;
			}
		}
		if (type == TOK_VALUE) {
			scan = json_tokenize_value(tokens, loc);
			if (scan == -1)
				goto fail;
			input += scan - 1;
		}
		json_add_token(tokens, type, start, loc, scan);
		if (type == TOK_QUOTE)
			in_string = !in_string;

		input++;
	}
	json_add_token(tokens, TOK_EOF, start, loc, 0);

	return (tokens);

fail:
	json_error(cause, "tokenization error", loc);
	json_destroy_tokens(tokens);
	return (NULL);
}

/*
 * Tokenize a value from the input string. Strings are terminated by a '"', and
 * numbers/booleans are terminated by a ',', ']', '}', or whitespace.
 */
static int
json_tokenize_value(struct json_tokens *tokens, const char *loc)
{
	struct json_token	*prev;
	int			 i, scan = 0;

	if (tokens->size == 0)
		return (-1);
	prev = &tokens->toks[tokens->size - 1];

	if (prev->type == TOK_QUOTE) {
		while (loc[scan] != '"') {
			if (loc[scan] == '\0' || (u_char)loc[scan] < 0x20)
				return (-1);
			if (loc[scan] != '\\') {
				scan++;
				continue;
			}
			scan++;
			switch (loc[scan]) {
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
				scan++;
				break;
			case 'u':
				for (i = 1; i <= 4; i++) {
					if (!isxdigit((u_char)loc[scan + i]))
						return (-1);
				}
				scan += 5;
				break;
			default:
				return (-1);
			}
		}
	} else if (prev->type == TOK_COLON) {
		do {
			if (loc[scan] == '\0')
				return (-1);
			scan++;
		} while (loc[scan] != ']' && loc[scan] != '}' &&
		    loc[scan] != ',' && !isspace((u_char) loc[scan]));
	} else
		return (-1);

	return (scan);
}

/* Create a new token container. */
static struct json_tokens *
json_create_tokens(void)
{
	struct json_tokens	*tokens;

	tokens = xmalloc(sizeof *tokens);
	tokens->size = 0;
	tokens->capacity = 1024;
	tokens->toks = xmalloc(tokens->capacity * sizeof *tokens->toks);

	return (tokens);
}

/* Free a token container. */
static void
json_destroy_tokens(struct json_tokens *tokens)
{
	free(tokens->toks);
	tokens->toks = NULL;
	free(tokens);
}

/* Add a token to tokens. */
static void
json_add_token(struct json_tokens *tokens, enum json_token_type type,
    const char *input, const char *loc, int len)
{
	struct json_token	*tok;

	while (tokens->size >= tokens->capacity) {
		tokens->capacity *= 2;
		tokens->toks = xrealloc(tokens->toks,
		    sizeof *tokens->toks * tokens->capacity);
	}

	tok = &tokens->toks[tokens->size++];
	tok->type = type;
	tok->offset = loc - input;
	tok->len = len;
}

/* Create a node and assign given values. */
static struct json_node *
json_create_node(struct json_node *parent, enum json_node_type type,
    const char *key, void *val)
{
	struct json_node	*node;

	node = xcalloc(1, sizeof *node);
	node->parent = parent;
	if (key != NULL)
		node->key = xstrdup(key);
	node->type = type;
	if (type == NODE_OBJECT)
		RB_INIT(&node->fields);
	else if (type == NODE_ARRAY)
		TAILQ_INIT(&node->members);
	if (val != NULL)
		json_assign_value(node, val);

	return (node);
}

/* Destroy a node and all of the node's fields. */
void
json_destroy_node(struct json_node *node)
{
	struct json_node	*field, *field1, *member;

	if (node == NULL)
		return;

	switch (node->type) {
	case NODE_STRING:
		free(node->str);
		break;
	case NODE_NUMBER:
	case NODE_BOOLEAN:
		break;
	case NODE_OBJECT:
		RB_FOREACH_SAFE(field, json_fields, &node->fields, field1) {
			RB_REMOVE(json_fields, &node->fields, field);
			json_destroy_node(field);
		}
		break;
	case NODE_ARRAY:
		while (!TAILQ_EMPTY(&node->members)) {
			member = TAILQ_FIRST(&node->members);
			TAILQ_REMOVE(&node->members, member, aentry);
			json_destroy_node(member);
		}
		break;
	}

	if (node->key != NULL)
		free(node->key);
	free(node);
}

/* Assign a value to a node. */
static void
json_assign_value(struct json_node *node, void *val)
{
	struct json_node	*child = val;

	switch (node->type) {
	case NODE_STRING:
		node->str = val;
		break;
	case NODE_NUMBER:
		node->num = *(int64_t *)val;
		break;
	case NODE_BOOLEAN:
		node->boolean = *(int *)val;
		break;
	case NODE_OBJECT:
		RB_INSERT(json_fields, &node->fields, child);
		break;
	case NODE_ARRAY:
		TAILQ_INSERT_TAIL(&node->members, child, aentry);
		break;
	default:
		fatalx("unknown node type");
	}
}

/* Parse a stream of tokens into nodes. Consumes the tokens. */
static struct json_node *
json_parse_tokens(struct json_tokens **tokens, struct json_parse_ctx *pctx)
{
	struct json_token	*tok = (*tokens)->toks;
	struct json_node	*jn = NULL;

	if (tok->type == TOK_OPENOBJECT)
		jn = json_parse_object(&tok, pctx, NULL, NULL);
	else {
		json_error(pctx->cause, "expected object",
		    pctx->input + tok->offset);
		goto fail;
	}
	if (jn == NULL)
		goto fail;

	if (tok->type != TOK_EOF) {
		json_error(pctx->cause, "unexpected trailing data",
		    pctx->input + tok->offset);
		goto fail;
	}
	json_destroy_tokens(*tokens);
	*tokens = NULL;

	return (jn);

fail:
	if (jn != NULL)
		json_destroy_node(jn);
	json_destroy_tokens(*tokens);
	*tokens = NULL;
	return (NULL);
}

/* Parse and return a key string, and advance the token pointer. */
static char *
json_parse_key(struct json_token **tok, struct json_parse_ctx *pctx)
{
	int		 len;
	const char	*loc, *start = pctx->input + (*tok)->offset;
	char		*key;

	if ((*tok)->type != TOK_QUOTE)
		goto fail;
	(*tok)++;

	loc = pctx->input + (*tok)->offset;
	len = (*tok)->len;

	if ((*tok)->type != TOK_VALUE)
		goto fail;
	(*tok)++;
	if ((*tok)->type != TOK_QUOTE)
		goto fail;

	key = xstrndup(loc, len);
	(*tok)++;

	return (key);

fail:
	json_error(pctx->cause, "invalid key", start);
	return (NULL);
}

/* Parse an object value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_object(struct json_token **tok, struct json_parse_ctx *pctx,
    const char *key, struct json_node *parent)
{
	struct json_node	*object, *field;
	char			*fkey = NULL;
	u_char			*valstr;

	if ((*tok)->type != TOK_OPENOBJECT)
		return (NULL);

	pctx->depth++;
	if (pctx->depth > PARSE_DEPTH_MAX) {
		json_error(pctx->cause, "parse depth exceeded",
		    pctx->input + (*tok)->offset);
		return (NULL);
	}

	(*tok)++;

	object = json_create_node(parent, NODE_OBJECT, key, NULL);
	while ((*tok)->type != TOK_CLOSEOBJECT) {
		if ((fkey = json_parse_key(tok, pctx)) == NULL)
			goto fail;
		if (json_find(object, fkey) != NULL) {
			json_error(pctx->cause, "duplicate key",
			    pctx->input + (*tok)->offset);
			goto fail;
		}
		if ((*tok)->type != TOK_COLON) {
			json_error(pctx->cause, "missing colon",
			    pctx->input + (*tok)->offset);
			goto fail;
		}
		(*tok)++;

		switch ((*tok)->type) {
		case TOK_QUOTE:
			field = json_parse_string(tok, pctx, fkey, object);
			break;
		case TOK_VALUE:
			valstr = (u_char *)(pctx->input + (*tok)->offset);
			if ((*valstr == '-' && isdigit(valstr[1])) ||
			    isdigit(*valstr)) {
				field = json_parse_number(tok, pctx, fkey,
				    object);
			} else {
				field = json_parse_boolean(tok, pctx, fkey,
				    object);
			}
			break;
		case TOK_OPENOBJECT:
			field = json_parse_object(tok, pctx, fkey, object);
			break;
		case TOK_OPENARRAY:
			field = json_parse_array(tok, pctx, fkey, object);
			break;
		default:
			json_error(pctx->cause,
			    "unexpected value when parsing object",
			    pctx->input + (*tok)->offset);
			goto fail;
		}
		if (field == NULL)
			goto fail;

		json_assign_value(object, field);
		if ((*tok)->type == TOK_COMMA) {
			if ((*tok)[1].type == TOK_CLOSEOBJECT) {
				json_error(pctx->cause, "invalid object",
				    pctx->input + (*tok)->offset);
				goto fail;
			}
			(*tok)++;
		} else if ((*tok)->type != TOK_CLOSEOBJECT) {
			json_error(pctx->cause, "invalid object",
			    pctx->input + (*tok)->offset);
			goto fail;
		}
		free(fkey);
	}
	(*tok)++;
	pctx->depth--;
	return (object);

fail:
	if (fkey != NULL)
		free(fkey);
	json_destroy_node(object);
	return (NULL);
}

/* Parse an array value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_array(struct json_token **tok, struct json_parse_ctx *pctx,
    const char *key, struct json_node *parent)
{
	struct json_node	*array, *member;

	if ((*tok)->type != TOK_OPENARRAY)
		return (NULL);
	(*tok)++;

	array = json_create_node(parent, NODE_ARRAY, key, NULL);
	while ((*tok)->type != TOK_CLOSEARRAY) {
		switch ((*tok)->type) {
		case TOK_OPENOBJECT:
			member = json_parse_object(tok, pctx, NULL, array);
			break;
		default:
			json_error(pctx->cause, "invalid array member",
			    pctx->input + (*tok)->offset);
			goto fail;
		}
		if (member == NULL)
			goto fail;

		json_assign_value(array, member);

		if ((*tok)->type == TOK_COMMA) {
			if ((*tok)[1].type == TOK_CLOSEARRAY) {
				json_error(pctx->cause, "invalid array",
				    pctx->input + (*tok)->offset);
				goto fail;
			}
			(*tok)++;
		} else if ((*tok)->type != TOK_CLOSEARRAY) {
			json_error(pctx->cause, "invalid array",
			    pctx->input + (*tok)->offset);
			goto fail;
		}
	}
	(*tok)++;
	return (array);

fail:
	json_destroy_node(array);
	return (NULL);
}

/* Parse a string value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_string(struct json_token **tok, struct json_parse_ctx *pctx,
    const char *key, struct json_node *parent)
{
	const char	*loc, *start = pctx->input + (*tok)->offset;
	char		*str;
	int		 len;

	if ((*tok)->type != TOK_QUOTE)
		goto fail;
	(*tok)++;
	if ((*tok)->type != TOK_VALUE)
		goto fail;

	loc = pctx->input + (*tok)->offset;
	len = (*tok)->len;
	(*tok)++;

	if ((*tok)->type != TOK_QUOTE)
		goto fail;
	(*tok)++;

	str = xstrndup(loc, len);
	return (json_create_node(parent, NODE_STRING, key, str));

fail:
	json_error(pctx->cause, "invalid string", start);
	return (NULL);
}

/* Parse a number value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_number(struct json_token **tok, struct json_parse_ctx *pctx,
    const char *key, struct json_node *parent)
{
	const char	*start = pctx->input + (*tok)->offset;
	char		*endptr;
	int64_t		 num;
	int		 len = (*tok)->len;

	if ((start[0] == '0' && len != 1) ||
	    (start[0] == '-' && start[1] == '0' && len != 2))
		goto fail;

	errno = 0;
	num = strtoll(start, &endptr, 10);
	if (errno != 0 || endptr != start + len)
		goto fail;
	(*tok)++;

	return (json_create_node(parent, NODE_NUMBER, key, &num));

fail:
	json_error(pctx->cause, "invalid number", start);
	return (NULL);
}

/* Parse a boolean value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_boolean(struct json_token **tok, struct json_parse_ctx *pctx,
    const char *key, struct json_node *parent)
{
	int		 len = (*tok)->len, boolean;
	const char	*start = pctx->input + (*tok)->offset;

	if (strncmp(start, "true", len) == 0 && len == 4)
		boolean = 1;
	else if (strncmp(start, "false", len) == 0 && len == 5)
		boolean = 0;
	else
		goto fail;
	(*tok)++;

	return (json_create_node(parent, NODE_BOOLEAN, key, &boolean));

fail:
	json_error(pctx->cause, "invalid boolean", start);
	return (NULL);
}

/* Append a node as JSON. */
static void
json_string_append(struct evbuffer *buffer, struct json_node *node)
{
	struct json_node	*field, *member;
	const char	*s;
	int		 comma = 0;

	switch (node->type) {
	case NODE_STRING:
		evbuffer_add_printf(buffer, "\"%s\"", node->str);
		break;
	case NODE_NUMBER:
		evbuffer_add_printf(buffer, "%lld", (long long)node->num);
		break;
	case NODE_BOOLEAN:
		if (node->boolean)
			s = "true";
		else
			s = "false";
		evbuffer_add(buffer, s, strlen(s));
		break;
	case NODE_OBJECT:
		evbuffer_add(buffer, "{", 1);
		RB_FOREACH(field, json_fields, &node->fields) {
			if (comma)
				evbuffer_add(buffer, ",", 1);
			evbuffer_add_printf(buffer, "\"%s\":", field->key);
			json_string_append(buffer, field);
			comma = 1;
		}
		evbuffer_add(buffer, "}", 1);
		break;
	case NODE_ARRAY:
		evbuffer_add(buffer, "[", 1);
		TAILQ_FOREACH(member, &node->members, aentry) {
			if (comma)
				evbuffer_add(buffer, ",", 1);
			json_string_append(buffer, member);
			comma = 1;
		}
		evbuffer_add(buffer, "]", 1);
		break;
	}
}

/* Convert a node back to JSON. */
char *
json_to_string(struct json_node *node)
{
	struct evbuffer	*buffer;
	char		*out;

	if (node == NULL)
		return (NULL);
	buffer = evbuffer_new();
	if (buffer == NULL)
		fatalx("out of memory");
	json_string_append(buffer, node);
	out = xmemdup(EVBUFFER_DATA(buffer), EVBUFFER_LENGTH(buffer));
	evbuffer_free(buffer);
	return (out);
}
