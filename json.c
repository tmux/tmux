/* $OpenBSD$ */

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
 * Parse a subset of JSON.
 *
 * The subset accepted is:
 *
 * - Arrays may only hold objects.
 * - Numbers are 64-bit signed integers in base 10; there are no
 *   fractions and no exponents.
 * - There is no null, and a string may not be empty.
 * - Escapes are not decoded. '\' is only used to skip past \" pairs.
 * - A key may not appear twice in the same object. Note that because escapes
 *   are not decoded, duplicate keys may go undetected.
 */

#define TOKENS_MAX	1000
#define ERROR_CTX_LEN	8

/* JSON Token types. */
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
	enum json_token_type	 type;
	const char		*loc;
	int			 len;
};

/* JSON tokens. */
struct json_tokens {
	struct json_token	*toks;
	int			 size;
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

/* JSON node. */
struct json_node {
	enum json_node_type		 type;
	const char			*key;
	const char			*loc;
	struct json_node		*parent;
	union {
		const char		*str;
		int64_t			 num;
		int			 boolean;
		struct json_fields	 fields;
		struct json_members	 members;
	};
	RB_ENTRY(json_node)		 oentry;
	TAILQ_ENTRY(json_node)		 aentry;
};

static struct json_tokens *json_tokenize_input(const char *, char **);
static int		 json_tokenize_value(struct json_tokens *, const char *);
static struct json_tokens *json_create_tokens(void);
static void		 json_free_tokens(struct json_tokens *);
static int		 json_add_token(struct json_tokens *,
			     enum json_token_type, const char *, int);
static const struct json_token *json_tokens_tail(struct json_tokens *);
static void		 json_error(char **, const char *, const char *);

static struct json_node	*json_create_node(struct json_node *,
			     enum json_node_type, const char *, const char *,
			     const void *);
static void		 json_assign_value(struct json_node *, const void *);
static struct json_node *json_parse_tokens(struct json_tokens **, char **);
static const char	*json_parse_key(struct json_token **, char **);
static struct json_node	*json_parse_object(struct json_token **, const char *,
			     struct json_node *, char **);
static struct json_node	*json_parse_array(struct json_token **, const char *,
			     struct json_node *, char **);
static struct json_node	*json_parse_string(struct json_token **, const char *,
			     struct json_node *, char **);
static struct json_node	*json_parse_number(struct json_token **, const char *,
			     struct json_node *, char **);
static struct json_node	*json_parse_boolean(struct json_token **, const char *,
			     struct json_node *, char **);
static int		 json_node_cmp(struct json_node *, struct json_node *);
RB_GENERATE_STATIC(json_fields, json_node, oentry, json_node_cmp);

/* Comparator for the json_fields tree. */
static int
json_node_cmp(struct json_node *a, struct json_node *b)
{
	return (strcmp(a->key, b->key));
}

/* Parse an input string into JSON. */
struct json_node *
json_parse(const char *input, char **cause)
{
	struct json_tokens	*tokens;

	if ((tokens = json_tokenize_input(input, cause)) == NULL)
		return (NULL);

	return (json_parse_tokens(&tokens, cause));
}

/* Returns a field node from an object node. */
struct json_node *
json_find(const struct json_node *jn, const char *key)
{
	struct json_node	*node = (struct json_node *)jn, tmp = { 0 };

	if (jn->type != NODE_OBJECT)
		return (NULL);

	tmp.key = key;
	return (RB_FIND(json_fields, &node->fields, &tmp));
}

/* Returns the first member of an array node. */
struct json_node *
json_array_first(const struct json_node *jn)
{
	if (jn->type != NODE_ARRAY)
		return (NULL);

	return (TAILQ_FIRST(&jn->members));
}

/* Returns the next member of an array's member node. */
struct json_node *
json_array_next(const struct json_node *member)
{
	if (member == NULL || member->parent->type != NODE_ARRAY)
		return (NULL);

	return (TAILQ_NEXT(member, aentry));
}

/* Returns the string value from a node. */
int
json_get_string(struct json_node *jn, const char **s)
{
	if (jn->type != NODE_STRING)
		return (-1);

	*s = xstrdup(jn->str);
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
json_find_string(const struct json_node *jn, const char *key, const char **out,
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
			xasprintf(cause, "key \"%s\" expected STRING value",
			    key);
		return (-1);
	}
	*out = field->str;

	return (0);
}

/* Returns the number value from a given key in an object node. */
int
json_find_number(const struct json_node *jn, const char *key, int64_t *out,
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
			xasprintf(cause, "key \"%s\" expected NUMBER value",
			    key);
		return (-1);
	}
	*out = field->num;

	return (0);
}

/* Returns the boolean value from a given key in an object node. */
int
json_find_boolean(const struct json_node *jn, const char *key, int *out,
    char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_BOOLEAN) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected BOOLEAN value",
			    key);
		return (-1);
	}
	*out = field->boolean;

	return (0);
}

/* Returns the object value from a given key in an object node. */
int
json_find_object(const struct json_node *jn, const char *key,
    struct json_node **out, char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_OBJECT) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected OBJECT value",
			    key);
		return (-1);
	}
	*out = field;

	return (0);
}

/* Returns the array value from a given key in an object node. */
int
json_find_array(const struct json_node *jn, const char *key,
    struct json_node **out, char **cause)
{
	struct json_node	*field;

	if ((field = json_find(jn, key)) == NULL) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" not found", key);
		return (-1);
	}
	if (field->type != NODE_ARRAY) {
		if (cause != NULL)
			xasprintf(cause, "key \"%s\" expected ARRAY value",
			    key);
		return (-1);
	}
	*out = field;

	return (0);
}

/* Fill an error cause. */
static void
json_error(char **cause, const char *reason, const char *input)
{
	const char	*ellipsis = "...";
	int		 i;

	if (input == NULL || *input == '\0') {
		xasprintf(cause, "%s", reason);
		return;
	}

	for (i = 0; i < ERROR_CTX_LEN + 1; i++) {
		if (input[i] == '\0') {
			ellipsis = "";
			break;
		}
	}

	xasprintf(cause, "%s: %.*s%s", reason, ERROR_CTX_LEN, input, ellipsis);
}

/* Tokenize the json string. */
static struct json_tokens *
json_tokenize_input(const char *input, char **cause)
{
	struct json_tokens	*tokens = json_create_tokens();
	enum json_token_type	 type;
	const char		*loc;
	int			 scan;

	while (*input != '\0') {
		if (tokens->size >= TOKENS_MAX)
			goto fail;

		loc = input;
		scan = 1;

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
			scan = json_tokenize_value(tokens, loc);
			if (scan == -1)
				goto fail;
			input += scan - 1;
		}
		if (json_add_token(tokens, type, loc, scan) != 0)
			goto fail;

		input++;
	}
	if (json_add_token(tokens, TOK_EOF, NULL, 0) != 0)
		goto fail;

	return (tokens);

fail:
	json_error(cause, "tokenization error", input);
	json_free_tokens(tokens);
	return (NULL);
}

/*
 * Tokenize a value from the input string. Strings are terminated by a '"', and
 * numbers/booleans are terminated by a ',', ']', '}', or whitespace.
 */
static int
json_tokenize_value(struct json_tokens *tokens, const char *loc)
{
	const struct json_token	*prev = json_tokens_tail(tokens);
	int			 scan = 0, escaping = 0;

	if (prev == NULL)
		return (-1);
	if (prev->type == TOK_QUOTE) {
		do {
			if (loc[scan] == '\0')
				return (-1);
			if (loc[scan] == '\\' && !escaping)
				escaping = 1;
			else if (escaping)
				escaping = 0;
			scan++;
		} while (loc[scan] != '"' || escaping);
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
	tokens->toks = xmalloc(sizeof *tokens->toks * TOKENS_MAX);

	return (tokens);
}

/* Free a token container. */
static void
json_free_tokens(struct json_tokens *tokens)
{
	free(tokens->toks);
	tokens->toks = NULL;
	free(tokens);
}

/* Add a token to tokens. */
static int
json_add_token(struct json_tokens *tokens, enum json_token_type type,
    const char *loc, int len)
{
	struct json_token	*tok;

	if (tokens->size >= TOKENS_MAX)
		return (-1);

	tok = &tokens->toks[tokens->size++];
	tok->type = type;
	tok->loc = loc;
	tok->len = len;

	return (0);
}

/* Returns a reference to the last token. */
static const struct json_token *
json_tokens_tail(struct json_tokens *tokens)
{
	if (tokens->size == 0)
		return (NULL);
	return (&tokens->toks[tokens->size - 1]);
}

/* Create a node and assign given values. */
static struct json_node *
json_create_node(struct json_node *parent, enum json_node_type type,
    const char *key, const char *loc, const void *val)
{
	struct json_node	*node;

	node = xcalloc(1, sizeof *node);
	node->parent = parent;
	if (key != NULL)
		node->key = xstrdup(key);
	node->loc = loc;
	node->type = type;
	if (type == NODE_ARRAY)
		TAILQ_INIT(&node->members);
	else
		RB_INIT(&node->fields);
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
		free((char *)node->str);
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
		free((char *)node->key);
	free(node);
}

/* Assign a value to a node. */
static void
json_assign_value(struct json_node *node, const void *val)
{
	struct json_node	*child = (struct json_node *)val;

	switch (node->type) {
	case NODE_STRING:
		node->str = (const char *)val;
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
json_parse_tokens(struct json_tokens **tokens, char **cause)
{
	struct json_token	*toks = (*tokens)->toks;
	struct json_node	*json = NULL;

	if (toks->type == TOK_OPENOBJECT)
		json = json_parse_object(&toks, NULL, NULL, cause);
	else {
		json_error(cause, "expected object", toks->loc);
		goto fail;
	}

	if (json == NULL)
		goto fail;

	if (toks->type != TOK_EOF) {
		json_error(cause, "unexpected trailing data", toks->loc);
		goto fail;
	}
	json_free_tokens(*tokens);
	*tokens = NULL;

	return (json);

fail:
	if (json != NULL)
		json_destroy_node(json);
	json_free_tokens(*tokens);
	*tokens = NULL;
	return (NULL);
}

/* Parse and return a key string, and advance the token pointer. */
static const char *
json_parse_key(struct json_token **toks, char **cause)
{
	const char	*key, *loc = (*toks)->loc;
	int		 len;

	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;

	loc = (*toks)->loc;
	len = (*toks)->len;

	if ((*toks)->type != TOK_VALUE)
		goto fail;
	(*toks)++;
	if ((*toks)->type != TOK_QUOTE)
		goto fail;

	key = xstrndup(loc, len);
	(*toks)++;

	return (key);

fail:
	json_error(cause, "invalid key", loc);
	return (NULL);
}

/* Parse an object value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_object(struct json_token **toks, const char *key,
    struct json_node *parent, char **cause)
{
	struct json_node	*object, *field;
	const char 		*loc, *fkey = NULL;
	u_char			*valstr;

	if ((*toks)->type != TOK_OPENOBJECT)
		return (NULL);
	loc = (*toks)->loc;
	(*toks)++;

	object = json_create_node(parent, NODE_OBJECT, key, loc, NULL);
	while ((*toks)->type != TOK_CLOSEOBJECT) {
		if ((fkey = json_parse_key(toks, cause)) == NULL)
			goto fail;
		if (json_find(object, fkey) != NULL) {
			json_error(cause, "duplicate key", (*toks)->loc);
			goto fail;
		}
		if ((*toks)->type != TOK_COLON) {
			json_error(cause, "missing colon", (*toks)->loc);
			goto fail;
		}
		(*toks)++;

		switch ((*toks)->type) {
		case TOK_QUOTE:
			field = json_parse_string(toks, fkey, object, cause);
			break;
		case TOK_VALUE:
			valstr = (u_char *)(*toks)->loc;
			if ((*valstr == '-' && isdigit(valstr[1])) ||
			    isdigit(*valstr)) {
				field = json_parse_number(toks, fkey, object,
				    cause);
			} else
				field = json_parse_boolean(toks, fkey, object,
				    cause);
			break;
		case TOK_OPENOBJECT:
			field = json_parse_object(toks, fkey, object, cause);
			break;
		case TOK_OPENARRAY:
			field = json_parse_array(toks, fkey, object, cause);
			break;
		default:
			json_error(cause, "unsupported object token",
			    (*toks)->loc);
			goto fail;
		}
		if (field == NULL)
			goto fail;

		json_assign_value(object, field);
		if ((*toks)->type == TOK_COMMA) {
			if ((*toks)[1].type == TOK_CLOSEOBJECT) {
				json_error(cause, "invalid object",
				    (*toks)->loc);
				goto fail;
			}
			(*toks)++;
		} else if ((*toks)->type != TOK_CLOSEOBJECT) {
			json_error(cause, "invalid object", (*toks)->loc);
			goto fail;
		}
		free((char *)fkey);
	}
	(*toks)++;
	return (object);

fail:
	if (fkey != NULL)
		free((char *)fkey);
	json_destroy_node(object);
	return (NULL);
}

/* Parse an array value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_array(struct json_token **toks, const char *key,
    struct json_node *parent, char **cause)
{
	struct json_node	*array, *member;
	const char		*loc;

	if ((*toks)->type != TOK_OPENARRAY)
		return (NULL);
	loc = (*toks)->loc;
	(*toks)++;

	array = json_create_node(parent, NODE_ARRAY, key, loc, NULL);
	while ((*toks)->type != TOK_CLOSEARRAY) {
		switch ((*toks)->type) {
		case TOK_OPENOBJECT:
			member = json_parse_object(toks, NULL, array, cause);
			break;
		default:
			json_error(cause, "invalid array member", (*toks)->loc);
			goto fail;
		}
		if (member == NULL)
			goto fail;

		json_assign_value(array, member);

		if ((*toks)->type == TOK_COMMA) {
			if ((*toks)[1].type == TOK_CLOSEARRAY) {
				json_error(cause, "invalid array", (*toks)->loc);
				goto fail;
			}
			(*toks)++;
		} else if ((*toks)->type != TOK_CLOSEARRAY) {
			json_error(cause, "invalid array", (*toks)->loc);
			goto fail;
		}
	}
	(*toks)++;
	return (array);

fail:
	json_destroy_node(array);
	return (NULL);
}

/* Parse a string value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_string(struct json_token **toks, const char *key,
    struct json_node *parent, char **cause)
{
	const char	*str, *start = (*toks)->loc;
	int		 len;

	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;
	if ((*toks)->type != TOK_VALUE)
		goto fail;

	str = (*toks)->loc;
	len = (*toks)->len;
	(*toks)++;

	if ((*toks)->type != TOK_QUOTE)
		goto fail;
	(*toks)++;

	str = xstrndup(str, len);
	return (json_create_node(parent, NODE_STRING, key, start, str));

fail:
	json_error(cause, "invalid string", start);
	return (NULL);
}

/* Parse a number value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_number(struct json_token **toks, const char *key,
    struct json_node *parent, char **cause)
{
	const char	*start = (*toks)->loc;
	char		*endptr;
	int64_t		 num;

	errno = 0;
	num = strtoll(start, &endptr, 10);
	if (errno != 0 || endptr != start + (*toks)->len)
		goto fail;
	(*toks)++;

	return (json_create_node(parent, NODE_NUMBER, key, start, &num));

fail:
	json_error(cause, "invalid number", start);
	return (NULL);
}

/* Parse a boolean value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_boolean(struct json_token **toks, const char *key,
    struct json_node *parent, char **cause)
{
	const char	*start = (*toks)->loc;
	int		 len = (*toks)->len, boolean;

	if (strncmp(start, "true", len) == 0 && len == 4)
		boolean = 1;
	else if (strncmp(start, "false", len) == 0 && len == 5)
		boolean = 0;
	else
		goto fail;

	(*toks)++;

	return (json_create_node(parent, NODE_BOOLEAN, key, start, &boolean));

fail:
	json_error(cause, "invalid boolean", start);
	return (NULL);
}
