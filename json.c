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
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "tmux.h"

#define TOKENS_MAX	4096

/* JSON string view. */
struct json_string {
	const char	*ptr;
	int		 len;
};

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
	enum json_token_type	type;
	struct json_string	val;
};

/* JSON tokens. */
struct json_tokens {
	struct json_token	*toks;
	int			 size;
};

static struct json_tokens *json_tokenize_input(const char *, char **);
static int		 json_tokenize_value(struct json_tokens *, const char *,
			     struct json_string *);
static struct json_tokens *json_create_tokens(void);
static void		 json_free_tokens(struct json_tokens *);
static int		 json_add_token(struct json_tokens *,
			     enum json_token_type, struct json_string *);
static const struct json_token *json_tokens_tail(struct json_tokens *);

static struct json_node	*json_create_node(struct json_node *,
			     enum json_node_type, struct json_string *,
			     const void *);
static void		 json_destroy_node(struct json_node *);
static void		 json_assign_value(struct json_node *, const void *);
static struct json_node *json_parse_tokens(struct json_tokens **);
static int		 layout_parse_key(struct json_token **,
			     struct json_string *);
static struct json_node	*json_parse_object(struct json_token **,
			     struct json_node *, struct json_string *);
static struct json_node	*json_parse_array(struct json_token **,
			     struct json_node *, struct json_string *);
static struct json_node	*json_parse_string(struct json_token **,
			     struct json_node *, struct json_string *);
static struct json_node	*json_parse_number(struct json_token **,
			     struct json_node *, struct json_string *);
static struct json_node	*json_parse_boolean(struct json_token **,
			     struct json_node *, struct json_string *);

/* Wrapper for string views with strcmp semantics. */
static int
jstrcmp(const struct json_string *jstr, const char *s)
{
	size_t	slen;

	slen = strlen(s);
	if ((size_t)jstr->len < slen)
		return (-1);
	if ((size_t)jstr->len > slen)
		return (1);

	return (memcmp(jstr->ptr, s, lsv->len));
}

/* Tokenize the json string. */
static struct json_tokens *
json_tokenize_input(const char *input, char **cause)
{
	struct json_tokens	*tokens = json_create_tokens();
	enum json_token_type	 type;
	struct json_string	 jstr = { 0 };
	int			 scan;

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
			scan = json_tokenize_value(tokens, input, &jstr);
			if (scan == -1)
				goto fail;
			input += scan - 1;
		}
		if (json_add_token(tokens, type, &jstr) != 0)
			goto fail;

		jstr.ptr = NULL;
		jstr.len = 0;
		input++;
	}
	if (json_add_token(tokens, TOK_EOF, NULL) != 0)
		goto fail;

	return (tokens);
fail:
	json_free_tokens(tokens);
	return (NULL);
}

/*
 * Tokenize a value from the input string. Strings are terminated by a '"', and
 * numbers/booleans are terminated by a ',', ']', or '}'.
 */
static int
json_tokenize_value(struct json_tokens *tokens, const char *input,
    struct json_string *jstr)
{
	const struct json_token	*prev = json_tokens_tail(tokens);
	int				 scan = 0, escaping = 0;

	if (prev == NULL)
		return (-1);
	if (prev->type == TOK_QUOTE) {
		do {
			if (input[scan] == '\0')
				return (-1);
			if (input[scan] == '\\' && !escaping)
				escaping = 1;
			else if (escaping)
				escaping = 0;
			scan++;
		} while (input[scan] != '"' && !escaping);
	} else if (prev->type == TOK_COLON) {
		do {
			if (input[scan] == '\0')
				return (-1);
			scan++;
		} while (input[scan] != ']' && input[scan] != '}' &&
		    input[scan] != ',' && !isspace(input[scan]));
	} else
		return (-1);

	jstr->ptr = input;
	jstr->len = scan;

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
    struct json_string *jstr)
{
	struct json_token	*tok;

	if (tokens->size >= TOKENS_MAX)
		return (-1);

	tok = &tokens->toks[tokens->size++];
	tok->type = type;
	if (jstr != NULL) {
		tok->val.ptr = jstr->ptr;
		tok->val.len = jstr->len;
	}

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
    struct json_string *key, const void *val)
{
	struct json_node	*node;

	node = xcalloc(1, sizeof *node);
	node->parent = parent;
	if (key != NULL) {
		node->key.ptr = key->ptr;
		node->key.len = key->len;
	}
	node->type = type;
	TAILQ_INIT(&node->val.fields);
	if (val != NULL)
		json_assign_value(node, val);

	return (node);
}

/* Assign a value to a node. */
static void
json_assign_value(struct json_node *node, const void *val)
{
	struct json_node		*child;
	struct json_string	*jstr;

	switch (node->type) {
	case NODE_STRING:
		jstr = (struct json_string *)val;
		node->val.jstr.ptr = lsv->ptr;
		node->val.jstr.len = lsv->len;
		break;
	case NODE_NUMBER:
		node->val.num = *(int64_t *)val;
		break;
	case NODE_BOOLEAN:
		node->val.bool = *(int *)val;
		break;
	case NODE_OBJECT:
	case NODE_ARRAY:
		child = (struct json_node *)val;
		TAILQ_INSERT_TAIL(&node->val.fields, child, entry);
		break;
	default:
		fatalx("unknown node type");
	}
}

/* Destroy a node and all of the node's fields. */
static void
json_destroy_node(struct json_node *node)
{
	struct json_node	*field;

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
			json_destroy_node(field);
		}
		break;
	}

	free(node);
}

struct json_node *
json_parse(const char *input, char **cause)
{
	struct json_tokens	*tokens;
	struct json_node	*json;

	if ((tokens = json_tokenize_input(input, cause)) == NULL)
		return (NULL);

	if ((json = json_parse_tokens(tokens, cause)) == NULL)
		return (NULL);
	
	return (json);
}

/* Parse a stream of tokens into nodes. Consumes the tokens. */
static struct json_node *
json_parse_tokens(struct json_tokens **tokens)
{
	struct json_token	*toks = (*tokens)->toks;
	struct json_node	*layout;

	if (toks->type == TOK_OPENOBJECT)
		layout = json_parse_object(&toks, NULL, NULL);
	else
		goto fail;

	if (layout == NULL)
		goto fail;

	if (toks->type != TOK_EOF) {
		json_destroy_node(layout);
		layout = NULL;
	}
	json_free_tokens(*tokens);
	*tokens = NULL;

	return (layout);
fail:
	json_free_tokens(*tokens);
	*tokens = NULL;
	return (NULL);
}

/* Parse and return a key string, and advance the token pointer. */
static int
layout_parse_key(struct json_token **toks, struct json_string *jstr)
{
	if ((*toks)->type != TOK_QUOTE)
		return (-1);
	(*toks)++;
	if ((*toks)->type != TOK_VALUE)
		return (-1);

	jstr->ptr = (*toks)->val.ptr;
	jstr->len = (*toks)->val.len;

	(*toks)++;
	if ((*toks)->type != TOK_QUOTE)
		return (-1);

	(*toks)++;

	return (0);
}

/* Parse an object value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_object(struct json_token **toks, struct json_node *parent,
    struct json_string *key)
{
	struct json_node		*object, *field;
	struct json_string	 fkey;

	if ((*toks)->type != TOK_OPENOBJECT)
		return (NULL);
	(*toks)++;

	object = json_create_node(parent, NODE_OBJECT, key, NULL);
	while ((*toks)->type != TOK_CLOSEOBJECT) {
		if (layout_parse_key(toks, &fkey) != 0)
			goto fail;
		if ((*toks)->type != TOK_COLON)
			goto fail;
		(*toks)++;

		switch ((*toks)->type) {
		case TOK_QUOTE:
			field = json_parse_string(toks, object, &fkey);
			break;
		case TOK_VALUE:
			if (jstrcmp(&(*toks)->val, "true") == 0 ||
			    jstrcmp(&(*toks)->val, "false") == 0) {
				field = json_parse_boolean(toks, object,
				    &fkey);
			} else {
				field = json_parse_number(toks, object,
				    &fkey);
			}
			break;
		case TOK_OPENOBJECT:
			field = json_parse_object(toks, object, &fkey);
			break;
		case TOK_OPENARRAY:
			field = json_parse_array(toks, object, &fkey);
			break;
		default:
			goto fail;
		}
		if (field == NULL)
			goto fail;
		json_assign_value(object, field);

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
	json_destroy_node(object);
	return (NULL);
}

/* Parse an array value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_array(struct json_token **toks, struct json_node *parent,
    struct json_string *key)
{
	struct json_node	*array;
	struct json_node	*member;

	if ((*toks)->type != TOK_OPENARRAY)
		return (NULL);
	(*toks)++;

	array = json_create_node(parent, NODE_ARRAY, key, NULL);
	while ((*toks)->type != TOK_CLOSEARRAY) {
		switch ((*toks)->type) {
		case TOK_OPENOBJECT:
			member = json_parse_object(toks, array, NULL);
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
		json_assign_value(array, member);

		if ((*toks)->type != TOK_COMMA &&
		    (*toks)->type != TOK_CLOSEARRAY)
			goto fail;
	}
	(*toks)++;
	return (array);
fail:
	json_destroy_node(array);
	return (NULL);
}

/* Parse a string value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_string(struct json_token **toks, struct json_node *parent,
    struct json_string *key)
{
	struct json_string	*val;

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

	return (json_create_node(parent, NODE_STRING, key, val));
}

/* Parse a number value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_number(struct json_token **toks, struct json_node *parent,
    struct json_string *key)
{
	const char	*numstr = (*toks)->val.ptr;
	char		*endptr;
	int64_t		 val;

	errno = 0;
	val = strtoll(numstr, &endptr, 10);
	if (errno != 0 || endptr != numstr + (*toks)->val.len)
		return (NULL);
	(*toks)++;

	return (json_create_node(parent, NODE_NUMBER, key, &val));
}

/* Parse a boolean value, return the node, and advance the token pointer. */
static struct json_node *
json_parse_boolean(struct json_token **toks, struct json_node *parent,
    struct json_string *key)
{
	int		val;

	if (jstrcmp(&(*toks)->val, "true") == 0)
		val = 1;
	else if (jstrcmp(&(*toks)->val, "false") == 0)
		val = 0;
	else
		return (NULL);

	(*toks)++;

	return (json_create_node(parent, NODE_BOOLEAN, key, &val));
}

/* Return 1 when the node's key is equal to the parameter. */
static int
json_key_is_eq(const struct json_node *field, const char *key)
{
	return (jstrcmp(&field->key, key) == 0);
}

/* Return 1 when the node's value is equal to the parameter. */
static int
json_val_is_eq(const struct json_node *field, const void *val)
{
	switch (field->type) {
	case NODE_STRING:
		return (jstrcmp(&field->val.jstr, val) == 0);
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
