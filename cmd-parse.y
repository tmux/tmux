/* $OpenBSD$ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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

%{

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "tmux.h"
#include "tmux-parser.h"

static int			 yylex(void);
static int			 yyparse(void);
static void printflike(1,2)	 yyerror(const char *, ...);

#define CMD_PARSE_MAX_ENVIRON_LEN 16384

/*
 * A node in the parse tree. Subtypes are distinguished by the type, never by
 * flags. The value holds the assignment name, the literal text, the
 * environment variable name, or the tilde user as appropriate; it is NULL
 * otherwise. Containers store ordered children.
 */
TAILQ_HEAD(cmd_parse_nodes, cmd_parse_node);

struct cmd_parse_node {
	enum cmd_parse_node_type	 type;
	u_int				 line;
	u_int				 end_line;
	char				*value;

	struct cmd_parse_nodes		 children;
	TAILQ_ENTRY(cmd_parse_node)	 entry;
};

struct cmd_parse_tree {
	int				 references;
	char				*file;
	int				 flags;
	struct cmd_parse_node		*root;
};

struct cmd_parse_state {
	FILE				*f;

	const char			*buf;
	size_t				 len;
	size_t				 off;

	int				 condition;
	int				 eol;
	int				 eof;
	struct cmd_parse_input		*input;
	u_int				 escapes;

	char				*error;
	struct cmd_parse_nodes		*commands;
};
static struct cmd_parse_state parse_state;

/* Builder used by the lexer while collecting the parts of a string token. */
struct cmd_parse_scan {
	struct cmd_parse_node		*node;
	char				*text;
	size_t				 len;
};

static char	*cmd_parse_get_error(const char *, u_int, const char *);
static struct cmd_parse_node	*cmd_parse_new_node(enum cmd_parse_node_type,
		    u_int);
static struct cmd_parse_node	*cmd_parse_copy_node(struct cmd_parse_node *);
static struct cmd_parse_nodes	*cmd_parse_new_nodes(void);
static void	 cmd_parse_free_node(struct cmd_parse_node *);
static void	 cmd_parse_append(struct cmd_parse_nodes *,
		    struct cmd_parse_nodes *);
static struct cmd_parse_nodes	*cmd_parse_wrap(struct cmd_parse_node *);
static struct cmd_parse_node	*cmd_parse_make_assign(enum cmd_parse_node_type,
		    struct cmd_parse_node *);
static struct cmd_parse_node	*cmd_parse_token_from_string(const char *);
static void	 cmd_parse_onegroup(struct cmd_parse_node *);
static void	 cmd_parse_print_sequence(char **, struct cmd_parse_node *,
		    u_int);
static void	 cmd_parse_print_item(char **, struct cmd_parse_node *, u_int,
		    int);

%}

%union
{
	struct cmd_parse_node		*node;
	struct cmd_parse_nodes		*nodes;
}

%token ERROR
%token HIDDEN
%token IF
%token ELSE
%token ELIF
%token ENDIF
%token <node> FORMAT TOKEN EQUALS

%type <node> expanded format argument command_name
%type <node> assign optional_assign hidden_assign
%type <node> commands condition condition1
%type <node> if_open if_elif if_else
%type <nodes> statements statement arguments argument_statements
%type <nodes> command elif elif1

%%

lines	: /* empty */
	| statements
	{
		struct cmd_parse_state	*ps = &parse_state;

		ps->commands = $1;
	}

statements	: statement '\n'
		{
			$$ = $1;
		}
		| statements statement '\n'
		{
			$$ = $1;
			cmd_parse_append($$, $2);
		}

statement	: /* empty */
		{
			$$ = cmd_parse_new_nodes();
		}
		| hidden_assign
		{
			$$ = cmd_parse_wrap($1);
		}
		| condition
		{
			$$ = cmd_parse_wrap($1);
		}
		| commands
		{
			$$ = cmd_parse_new_nodes();
			if (TAILQ_EMPTY(&$1->children))
				cmd_parse_free_node($1);
			else
				TAILQ_INSERT_TAIL($$, $1, entry);
		}

format		: FORMAT
		{
			$$ = $1;
		}
		| TOKEN
		{
			$$ = $1;
		}

expanded	: format
		{
			$$ = $1;
		}

optional_assign	: /* empty */
		{
			$$ = NULL;
		}
		| assign
		{
			$$ = $1;
		}

assign	: EQUALS
	{
		$$ = cmd_parse_make_assign(CMD_PARSE_ASSIGN, $1);
	}

hidden_assign : HIDDEN EQUALS
		{
			$$ = cmd_parse_make_assign(CMD_PARSE_HIDDEN_ASSIGN, $2);
		}

if_open	: IF expanded
	{
		struct cmd_parse_state	*ps = &parse_state;

		$$ = cmd_parse_new_node(CMD_PARSE_IF, ps->input->line);
		TAILQ_INSERT_TAIL(&$$->children, $2, entry);
	}

if_else	: ELSE
	{
		struct cmd_parse_state	*ps = &parse_state;

		$$ = cmd_parse_new_node(CMD_PARSE_ELSE, ps->input->line);
	}

if_elif	: ELIF expanded
	{
		struct cmd_parse_state	*ps = &parse_state;

		$$ = cmd_parse_new_node(CMD_PARSE_ELIF, ps->input->line);
		TAILQ_INSERT_TAIL(&$$->children, $2, entry);
	}

condition	: if_open '\n' statements ENDIF
		{
			cmd_parse_append(&$1->children, $3);
			$$ = $1;
		}
		| if_open '\n' statements if_else '\n' statements ENDIF
		{
			cmd_parse_append(&$1->children, $3);
			cmd_parse_append(&$4->children, $6);
			TAILQ_INSERT_TAIL(&$1->children, $4, entry);
			$$ = $1;
		}
		| if_open '\n' statements elif ENDIF
		{
			cmd_parse_append(&$1->children, $3);
			cmd_parse_append(&$1->children, $4);
			$$ = $1;
		}
		| if_open '\n' statements elif if_else '\n' statements ENDIF
		{
			cmd_parse_append(&$1->children, $3);
			cmd_parse_append(&$1->children, $4);
			cmd_parse_append(&$5->children, $7);
			TAILQ_INSERT_TAIL(&$1->children, $5, entry);
			$$ = $1;
		}

elif	: if_elif '\n' statements
	{
		cmd_parse_append(&$1->children, $3);
		$$ = cmd_parse_new_nodes();
		TAILQ_INSERT_TAIL($$, $1, entry);
	}
	| if_elif '\n' statements elif
	{
		cmd_parse_append(&$1->children, $3);
		$$ = cmd_parse_new_nodes();
		TAILQ_INSERT_TAIL($$, $1, entry);
		cmd_parse_append($$, $4);
	}

commands	: command
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_input	*pi = ps->input;

			$$ = cmd_parse_new_node(CMD_PARSE_SEQUENCE, pi->line);
			cmd_parse_append(&$$->children, $1);
		}
		| commands ';'
		{
			$$ = $1;
		}
		| commands ';' condition1
		{
			$$ = $1;
			TAILQ_INSERT_TAIL(&$$->children, $3, entry);
		}
		| commands ';' command
		{
			$$ = $1;
			cmd_parse_append(&$$->children, $3);
		}
		| condition1
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_input	*pi = ps->input;

			$$ = cmd_parse_new_node(CMD_PARSE_SEQUENCE, pi->line);
			TAILQ_INSERT_TAIL(&$$->children, $1, entry);
		}

command	: assign
	{
		$$ = cmd_parse_new_nodes();
		TAILQ_INSERT_TAIL($$, $1, entry);
	}
	| optional_assign command_name
	{
		struct cmd_parse_state	*ps = &parse_state;
		struct cmd_parse_node	*cmd;
		struct cmd_parse_input	*pi = ps->input;

		cmd = cmd_parse_new_node(CMD_PARSE_COMMAND, pi->line);
		TAILQ_INSERT_TAIL(&cmd->children, $2, entry);

		$$ = cmd_parse_new_nodes();
		if ($1 != NULL)
			TAILQ_INSERT_TAIL($$, $1, entry);
		TAILQ_INSERT_TAIL($$, cmd, entry);
	}
	| optional_assign command_name arguments
	{
		struct cmd_parse_state	*ps = &parse_state;
		struct cmd_parse_node	*cmd;
		struct cmd_parse_input	*pi = ps->input;

		cmd = cmd_parse_new_node(CMD_PARSE_COMMAND, pi->line);
		TAILQ_INSERT_TAIL(&cmd->children, $2, entry);
		cmd_parse_append(&cmd->children, $3);

		$$ = cmd_parse_new_nodes();
		if ($1 != NULL)
			TAILQ_INSERT_TAIL($$, $1, entry);
		TAILQ_INSERT_TAIL($$, cmd, entry);
	}

command_name	: TOKEN
		{
			$$ = $1;
		}

condition1	: if_open commands ENDIF
		{
			TAILQ_INSERT_TAIL(&$1->children, $2, entry);
			$$ = $1;
		}
		| if_open commands if_else commands ENDIF
		{
			TAILQ_INSERT_TAIL(&$1->children, $2, entry);
			TAILQ_INSERT_TAIL(&$3->children, $4, entry);
			TAILQ_INSERT_TAIL(&$1->children, $3, entry);
			$$ = $1;
		}
		| if_open commands elif1 ENDIF
		{
			TAILQ_INSERT_TAIL(&$1->children, $2, entry);
			cmd_parse_append(&$1->children, $3);
			$$ = $1;
		}
		| if_open commands elif1 if_else commands ENDIF
		{
			TAILQ_INSERT_TAIL(&$1->children, $2, entry);
			cmd_parse_append(&$1->children, $3);
			TAILQ_INSERT_TAIL(&$4->children, $5, entry);
			TAILQ_INSERT_TAIL(&$1->children, $4, entry);
			$$ = $1;
		}

elif1	: if_elif commands
	{
		TAILQ_INSERT_TAIL(&$1->children, $2, entry);
		$$ = cmd_parse_new_nodes();
		TAILQ_INSERT_TAIL($$, $1, entry);
	}
	| if_elif commands elif1
	{
		TAILQ_INSERT_TAIL(&$1->children, $2, entry);
		$$ = cmd_parse_new_nodes();
		TAILQ_INSERT_TAIL($$, $1, entry);
		cmd_parse_append($$, $3);
	}

arguments	: argument
		{
			$$ = cmd_parse_new_nodes();
			TAILQ_INSERT_HEAD($$, $1, entry);
		}
		| argument arguments
		{
			TAILQ_INSERT_HEAD($2, $1, entry);
			$$ = $2;
		}

argument	: TOKEN
		{
			$$ = $1;
		}
		| EQUALS
		{
			$$ = $1;
		}
		| '{' argument_statements
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_input	*pi = ps->input;

			$$ = cmd_parse_new_node(CMD_PARSE_COMMANDS, pi->line);
			cmd_parse_append(&$$->children, $2);
		}

argument_statements	: statement '}'
			{
				$$ = $1;
			}
			| statements statement '}'
			{
				$$ = $1;
				cmd_parse_append($$, $2);
			}

%%

static char *
cmd_parse_get_error(const char *file, u_int line, const char *error)
{
	char	*s;

	if (file == NULL)
		s = xstrdup(error);
	else
		xasprintf(&s, "%s:%u: %s", file, line, error);
	return (s);
}

static struct cmd_parse_node *
cmd_parse_new_node(enum cmd_parse_node_type type, u_int line)
{
	struct cmd_parse_node	*node;

	node = xcalloc(1, sizeof *node);
	node->type = type;
	node->line = line;
	node->end_line = line;
	TAILQ_INIT(&node->children);
	return (node);
}

static struct cmd_parse_nodes *
cmd_parse_new_nodes(void)
{
	struct cmd_parse_nodes	*nodes;

	nodes = xmalloc(sizeof *nodes);
	TAILQ_INIT(nodes);
	return (nodes);
}

static void
cmd_parse_free_node(struct cmd_parse_node *node)
{
	struct cmd_parse_node	*child, *child1;

	if (node == NULL)
		return;

	TAILQ_FOREACH_SAFE(child, &node->children, entry, child1) {
		TAILQ_REMOVE(&node->children, child, entry);
		cmd_parse_free_node(child);
	}
	free(node->value);
	free(node);
}

/* Recursively copy a node and all of its descendants. */
static struct cmd_parse_node *
cmd_parse_copy_node(struct cmd_parse_node *node)
{
	struct cmd_parse_node	*new, *child, *copy;

	new = cmd_parse_new_node(node->type, node->line);
	new->end_line = node->end_line;
	if (node->value != NULL)
		new->value = xstrdup(node->value);

	TAILQ_FOREACH(child, &node->children, entry) {
		copy = cmd_parse_copy_node(child);
		TAILQ_INSERT_TAIL(&new->children, copy, entry);
	}
	return (new);
}

/* Move all nodes from src to the tail of dst, then free the src list head. */
static void
cmd_parse_append(struct cmd_parse_nodes *dst, struct cmd_parse_nodes *src)
{
	TAILQ_CONCAT(dst, src, entry);
	free(src);
}

/* Wrap a single item node in a new sequence and return a one-element list. */
static struct cmd_parse_nodes *
cmd_parse_wrap(struct cmd_parse_node *item)
{
	struct cmd_parse_nodes	*nodes;
	struct cmd_parse_node	*seq;

	seq = cmd_parse_new_node(CMD_PARSE_SEQUENCE, item->line);
	TAILQ_INSERT_TAIL(&seq->children, item, entry);

	nodes = cmd_parse_new_nodes();
	TAILQ_INSERT_TAIL(nodes, seq, entry);
	return (nodes);
}

/*
 * Turn a "NAME=value" string token into an assignment node. The leading TEXT
 * child is guaranteed by the lexer to begin with a valid name followed by '='.
 * The name becomes the node value and the remainder of the string (text after
 * '=' plus any following parts) becomes a string child.
 */
static struct cmd_parse_node *
cmd_parse_make_assign(enum cmd_parse_node_type type,
    struct cmd_parse_node *string)
{
	struct cmd_parse_node	*node, *value, *first, *child, *child1;
	const char		*cp, *eq;

	node = cmd_parse_new_node(type, string->line);

	first = TAILQ_FIRST(&string->children);
	eq = strchr(first->value, '=');
	node->value = xstrndup(first->value, eq - first->value);

	value = cmd_parse_new_node(CMD_PARSE_STRING, string->line);

	cp = eq + 1;
	if (*cp != '\0') {
		child = cmd_parse_new_node(CMD_PARSE_TEXT, string->line);
		child->value = xstrdup(cp);
		TAILQ_INSERT_TAIL(&value->children, child, entry);
	}
	TAILQ_REMOVE(&string->children, first, entry);
	cmd_parse_free_node(first);

	TAILQ_FOREACH_SAFE(child, &string->children, entry, child1) {
		TAILQ_REMOVE(&string->children, child, entry);
		TAILQ_INSERT_TAIL(&value->children, child, entry);
	}
	cmd_parse_free_node(string);

	TAILQ_INSERT_TAIL(&node->children, value, entry);
	return (node);
}

/* Build a string node containing a single literal text child. */
static struct cmd_parse_node *
cmd_parse_token_from_string(const char *s)
{
	struct cmd_parse_node	*node, *child;
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_input	*pi = ps->input;

	node = cmd_parse_new_node(CMD_PARSE_STRING, pi->line);
	if (*s != '\0') {
		child = cmd_parse_new_node(CMD_PARSE_TEXT, pi->line);
		child->value = xstrdup(s);
		TAILQ_INSERT_TAIL(&node->children, child, entry);
	}
	return (node);
}

/*
 * Merge consecutive sequence siblings into one at every node. This implements
 * CMD_PARSE_ONEGROUP: newlines no longer split sequences, so a string parsed
 * over several lines behaves as a single failure scope.
 */
static void
cmd_parse_onegroup(struct cmd_parse_node *node)
{
	struct cmd_parse_node	*child, *child1, *first = NULL;

	TAILQ_FOREACH(child, &node->children, entry)
		cmd_parse_onegroup(child);

	TAILQ_FOREACH_SAFE(child, &node->children, entry, child1) {
		if (child->type != CMD_PARSE_SEQUENCE) {
			first = NULL;
			continue;
		}
		if (first == NULL) {
			first = child;
			continue;
		}
		TAILQ_CONCAT(&first->children, &child->children, entry);
		TAILQ_REMOVE(&node->children, child, entry);
		cmd_parse_free_node(child);
	}
}

static struct cmd_parse_tree *
cmd_parse_run_parser(char **cause)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_node	*root;
	struct cmd_parse_tree	*tree;
	int			 retval;

	ps->commands = NULL;

	retval = yyparse();
	if (retval != 0) {
		*cause = ps->error;
		return (NULL);
	}

	root = cmd_parse_new_node(CMD_PARSE_ROOT, 0);
	if (ps->commands != NULL) {
		cmd_parse_append(&root->children, ps->commands);
		ps->commands = NULL;
	}
	if (ps->input->flags & CMD_PARSE_ONEGROUP)
		cmd_parse_onegroup(root);

	tree = xcalloc(1, sizeof *tree);
	tree->references = 1;
	tree->file = ps->input->file != NULL ? xstrdup(ps->input->file) : NULL;
	tree->flags = (ps->input->flags & ~CMD_PARSE_ONEGROUP);
	tree->root = root;
	return (tree);
}

static struct cmd_parse_tree *
cmd_parse_do_file(FILE *f, struct cmd_parse_input *pi, char **cause)
{
	struct cmd_parse_state	*ps = &parse_state;

	memset(ps, 0, sizeof *ps);
	ps->input = pi;
	ps->f = f;
	return (cmd_parse_run_parser(cause));
}

static struct cmd_parse_tree *
cmd_parse_do_buffer(const char *buf, size_t len, struct cmd_parse_input *pi,
    char **cause)
{
	struct cmd_parse_state	*ps = &parse_state;

	memset(ps, 0, sizeof *ps);
	ps->input = pi;
	ps->buf = buf;
	ps->len = len;
	return (cmd_parse_run_parser(cause));
}

struct cmd_parse_tree *
cmd_parse_from_file(FILE *f, struct cmd_parse_input *pi, char **cause)
{
	struct cmd_parse_input	 input;

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}
	*cause = NULL;
	return (cmd_parse_do_file(f, pi, cause));
}

struct cmd_parse_tree *
cmd_parse_from_buffer(const void *buf, size_t len, struct cmd_parse_input *pi,
    char **cause)
{
	struct cmd_parse_input	 input;

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}
	*cause = NULL;
	return (cmd_parse_do_buffer(buf, len, pi, cause));
}

struct cmd_parse_tree *
cmd_parse_from_string(const char *s, struct cmd_parse_input *pi, char **cause)
{
	struct cmd_parse_input	 input;

	if (pi != NULL)
		memcpy(&input, pi, sizeof input);
	else
		memset(&input, 0, sizeof input);

	/*
	 * When parsing a string, put commands in one group even if there are
	 * multiple lines. This means { a \n b } is identical to "a ; b" when
	 * given as an argument to another command.
	 */
	input.flags |= CMD_PARSE_ONEGROUP;
	return (cmd_parse_from_buffer(s, strlen(s), &input, cause));
}

struct cmd_parse_tree *
cmd_parse_from_node(struct cmd_parse_tree *tree, struct cmd_parse_node *node)
{
	struct cmd_parse_tree	*new;
	struct cmd_parse_node	*root, *child, *copy;

	root = cmd_parse_new_node(CMD_PARSE_ROOT, node->line);
	root->end_line = node->end_line;
	TAILQ_FOREACH(child, &node->children, entry) {
		copy = cmd_parse_copy_node(child);
		TAILQ_INSERT_TAIL(&root->children, copy, entry);
	}

	new = xcalloc(1, sizeof *new);
	new->references = 1;
	new->file = tree->file != NULL ? xstrdup(tree->file) : NULL;
	new->flags = tree->flags;
	new->root = root;
	return (new);
}

/* Build a string node with a single literal text child. */
static struct cmd_parse_node *
cmd_parse_new_string_node(const char *s, u_int line)
{
	struct cmd_parse_node	*string, *text;

	string = cmd_parse_new_node(CMD_PARSE_STRING, line);
	if (*s != '\0') {
		text = cmd_parse_new_node(CMD_PARSE_TEXT, line);
		text->value = xstrdup(s);
		TAILQ_INSERT_TAIL(&string->children, text, entry);
	}
	return (string);
}

/* Build a commands node from a copy of the children of a tree's root. */
static struct cmd_parse_node *
cmd_parse_new_commands_node(struct cmd_parse_tree *tree, u_int line)
{
	struct cmd_parse_node	*commands, *root, *child, *copy;

	commands = cmd_parse_new_node(CMD_PARSE_COMMANDS, line);

	root = cmd_parse_root(tree);
	TAILQ_FOREACH(child, &root->children, entry) {
		copy = cmd_parse_copy_node(child);
		TAILQ_INSERT_TAIL(&commands->children, copy, entry);
	}

	return (commands);
}

/* Add one argument to the current command, creating it if needed. */
static void
cmd_parse_from_arguments_add(struct cmd_parse_node *seq,
    struct cmd_parse_node **cmd, struct cmd_parse_node *child)
{
	if (*cmd == NULL) {
		*cmd = cmd_parse_new_node(CMD_PARSE_COMMAND, 0);
		TAILQ_INSERT_TAIL(&seq->children, *cmd, entry);
	}
	TAILQ_INSERT_TAIL(&(*cmd)->children, child, entry);
}

/* Build a parse tree directly from existing argument values. */
struct cmd_parse_tree *
cmd_parse_from_arguments(struct args_value *values, u_int count)
{
	struct cmd_parse_tree	*new;
	struct cmd_parse_node	*root, *seq, *cmd = NULL, *child;
	u_int			 i;
	char			*copy;
	size_t			 size;
	int			 end;

	root = cmd_parse_new_node(CMD_PARSE_ROOT, 0);
	seq = cmd_parse_new_node(CMD_PARSE_SEQUENCE, 0);
	TAILQ_INSERT_TAIL(&root->children, seq, entry);

	for (i = 0; i < count; i++) {
		end = 0;

		switch (values[i].type) {
		case ARGS_NONE:
			continue;
		case ARGS_STRING:
			copy = xstrdup(values[i].string);
			size = strlen(copy);

			if (size != 0 && copy[size - 1] == ';') {
				copy[--size] = '\0';
				if (size > 0 && copy[size - 1] == '\\')
					copy[size - 1] = ';';
				else
					end = 1;
			}

			if (!end || size != 0) {
				child = cmd_parse_new_string_node(copy, 0);
				cmd_parse_from_arguments_add(seq, &cmd, child);
			}
			free(copy);
			break;
		case ARGS_COMMANDS:
			child = cmd_parse_new_commands_node(values[i].cmd, 0);
			cmd_parse_from_arguments_add(seq, &cmd, child);
			break;
		default:
			fatalx("unknown argument type");
		}

		if (end)
			cmd = NULL;
	}

	new = xcalloc(1, sizeof *new);
	new->references = 1;
	new->root = root;
	return (new);
}

struct cmd_parse_tree *
cmd_parse_add_ref(struct cmd_parse_tree *tree)
{
	tree->references++;
	return (tree);
}

void
cmd_parse_free(struct cmd_parse_tree *tree)
{
	if (tree != NULL && --tree->references == 0) {
		cmd_parse_free_node(tree->root);
		free(tree->file);
		free(tree);
	}
}

struct cmd_parse_node *
cmd_parse_root(struct cmd_parse_tree *tree)
{
	return (tree->root);
}

const char *
cmd_parse_file(struct cmd_parse_tree *tree)
{
	return (tree->file);
}

int
cmd_parse_flags(struct cmd_parse_tree *tree)
{
	return (tree->flags);
}

enum cmd_parse_node_type
cmd_parse_node_type(struct cmd_parse_node *node)
{
	return (node->type);
}

const char *
cmd_parse_node_type_string(enum cmd_parse_node_type type)
{
	switch (type) {
	case CMD_PARSE_ROOT:
		return ("ROOT");
	case CMD_PARSE_SEQUENCE:
		return ("SEQUENCE");
	case CMD_PARSE_COMMAND:
		return ("COMMAND");
	case CMD_PARSE_STRING:
		return ("STRING");
	case CMD_PARSE_COMMANDS:
		return ("COMMANDS");
	case CMD_PARSE_TEXT:
		return ("TEXT");
	case CMD_PARSE_ENVIRONMENT:
		return ("ENVIRONMENT");
	case CMD_PARSE_TILDE:
		return ("TILDE");
	case CMD_PARSE_ASSIGN:
		return ("ASSIGN");
	case CMD_PARSE_HIDDEN_ASSIGN:
		return ("HIDDEN_ASSIGN");
	case CMD_PARSE_IF:
		return ("IF");
	case CMD_PARSE_ELIF:
		return ("ELIF");
	case CMD_PARSE_ELSE:
		return ("ELSE");
	}
	return ("UNKNOWN");
}

const char *
cmd_parse_node_value(struct cmd_parse_node *node)
{
	return (node->value);
}

u_int
cmd_parse_node_line(struct cmd_parse_node *node)
{
	return (node->line);
}

u_int
cmd_parse_node_end_line(struct cmd_parse_node *node)
{
	return (node->end_line);
}

struct cmd_parse_node *
cmd_parse_node_first_child(struct cmd_parse_node *node)
{
	return (TAILQ_FIRST(&node->children));
}

struct cmd_parse_node *
cmd_parse_node_next(struct cmd_parse_node *node)
{
	return (TAILQ_NEXT(node, entry));
}

/* Append a printf-style string to a growing buffer. */
static void printflike(2, 3)
cmd_parse_strcat(char **buf, const char *fmt, ...)
{
	va_list	 ap;
	char	*add, *new;

	va_start(ap, fmt);
	xvasprintf(&add, fmt, ap);
	va_end(ap);

	if (*buf == NULL)
		*buf = add;
	else {
		xasprintf(&new, "%s%s", *buf, add);
		free(*buf);
		free(add);
		*buf = new;
	}
}

static void
cmd_parse_log_one_node(const char *prefix, struct cmd_parse_node *node,
    u_int depth)
{
	struct cmd_parse_node	*child;
	const char		*type = cmd_parse_node_type_string(node->type);

	if (node->value == NULL)
		log_debug("%s: %*s%s", prefix, depth * 2, "", type);
	else {
		log_debug("%s: %*s%s value=\"%s\"", prefix, depth * 2, "",
		    type, node->value);
	}

	TAILQ_FOREACH(child, &node->children, entry)
		cmd_parse_log_one_node(prefix, child, depth + 1);
}

void
cmd_parse_log_node(const char *prefix, struct cmd_parse_node *node)
{
	cmd_parse_log_one_node(prefix, node, 0);
}

void
cmd_parse_log(const char *prefix, struct cmd_parse_tree *tree)
{
	cmd_parse_log_node(prefix, tree->root);
}

/* Does this literal text need quoting to reparse as itself? */
static int
cmd_parse_text_safe(const char *s)
{
	const char	*cp;

	if (*s == '\0')
		return (0);
	if (strchr("~#%", *s) != NULL)
		return (0);
	for (cp = s; *cp != '\0'; cp++) {
		if (isalnum((u_char)*cp))
			continue;
		if (strchr("_./:@+-,", *cp) == NULL)
			return (0);
	}
	return (1);
}

static int
cmd_parse_text_control(const char *s)
{
	for (; *s != '\0'; s++) {
		if ((u_char)*s < ' ' || (u_char)*s == 0x7f)
			return (1);
	}
	return (0);
}

static void
cmd_parse_print_text(char **buf, const char *s)
{
	const char	*cp;
	u_char		 ch;

	/* Plain text with no special characters can be used as is. */
	if (cmd_parse_text_safe(s)) {
		cmd_parse_strcat(buf, "%s", s);
		return;
	}

	/*
	 * Control bytes cannot survive single quotes (a literal newline is
	 * folded by the lexer), so escape with backslashes outside quotes.
	 */
	if (cmd_parse_text_control(s)) {
		for (cp = s; *cp != '\0'; cp++) {
			ch = (u_char)*cp;
			switch (ch) {
			case '\n':
				cmd_parse_strcat(buf, "\\n");
				break;
			case '\r':
				cmd_parse_strcat(buf, "\\r");
				break;
			case '\t':
				cmd_parse_strcat(buf, "\\t");
				break;
			default:
				if (ch < ' ' || ch == 0x7f)
					cmd_parse_strcat(buf, "\\%03o", ch);
				else if (strchr(" \"'$~;{}#%\\", ch) != NULL)
					cmd_parse_strcat(buf, "\\%c", ch);
				else
					cmd_parse_strcat(buf, "%c", ch);
				break;
			}
		}
		return;
	}

	/* Otherwise single quotes keep everything literal. */
	cmd_parse_strcat(buf, "'");
	for (cp = s; *cp != '\0'; cp++) {
		if (*cp == '\'')
			cmd_parse_strcat(buf, "'\\''");
		else
			cmd_parse_strcat(buf, "%c", *cp);
	}
	cmd_parse_strcat(buf, "'");
}

static void
cmd_parse_print_indent(char **buf, u_int depth)
{
	u_int	i;

	for (i = 0; i < depth; i++)
		cmd_parse_strcat(buf, "\t");
}

static void
cmd_parse_print_string(char **buf, struct cmd_parse_node *string)
{
	struct cmd_parse_node	*child;

	if (TAILQ_EMPTY(&string->children)) {
		cmd_parse_strcat(buf, "''");
		return;
	}

	TAILQ_FOREACH(child, &string->children, entry) {
		switch (child->type) {
		case CMD_PARSE_TEXT:
			cmd_parse_print_text(buf, child->value);
			break;
		case CMD_PARSE_ENVIRONMENT:
			cmd_parse_strcat(buf, "${%s}", child->value);
			break;
		case CMD_PARSE_TILDE:
			if (child->value != NULL && *child->value != '\0')
				cmd_parse_strcat(buf, "~%s", child->value);
			else
				cmd_parse_strcat(buf, "~");
			break;
		default:
			break;
		}
	}
}

static void
cmd_parse_print_commands(char **buf, struct cmd_parse_node *commands,
    u_int depth)
{
	struct cmd_parse_node	*child;

	if (TAILQ_EMPTY(&commands->children)) {
		cmd_parse_strcat(buf, "{}");
		return;
	}

	cmd_parse_strcat(buf, "{\n");
	TAILQ_FOREACH(child, &commands->children, entry) {
		cmd_parse_print_indent(buf, depth + 1);
		cmd_parse_print_sequence(buf, child, depth + 1);
		cmd_parse_strcat(buf, "\n");
	}
	cmd_parse_print_indent(buf, depth);
	cmd_parse_strcat(buf, "}");
}

static void
cmd_parse_print_command(char **buf, struct cmd_parse_node *cmd, u_int depth)
{
	struct cmd_parse_node	*child;
	int			 first = 1;

	TAILQ_FOREACH(child, &cmd->children, entry) {
		if (!first)
			cmd_parse_strcat(buf, " ");
		first = 0;
		if (child->type == CMD_PARSE_COMMANDS)
			cmd_parse_print_commands(buf, child, depth);
		else
			cmd_parse_print_string(buf, child);
	}
}

static void
cmd_parse_print_break(char **buf, u_int depth, int oneline)
{
	if (oneline)
		cmd_parse_strcat(buf, " ");
	else {
		cmd_parse_strcat(buf, "\n");
		cmd_parse_print_indent(buf, depth);
	}
}

static void
cmd_parse_print_if(char **buf, struct cmd_parse_node *node, u_int depth,
    int oneline)
{
	struct cmd_parse_node	*child, *sub;

	child = TAILQ_FIRST(&node->children);
	cmd_parse_strcat(buf, "%%if ");
	cmd_parse_print_string(buf, child);

	for (child = TAILQ_NEXT(child, entry); child != NULL;
	    child = TAILQ_NEXT(child, entry)) {
		switch (child->type) {
		case CMD_PARSE_SEQUENCE:
			cmd_parse_print_break(buf, depth, oneline);
			cmd_parse_print_sequence(buf, child, depth);
			break;
		case CMD_PARSE_ELIF:
			sub = TAILQ_FIRST(&child->children);
			cmd_parse_print_break(buf, depth, oneline);
			cmd_parse_strcat(buf, "%%elif ");
			cmd_parse_print_string(buf, sub);
			for (sub = TAILQ_NEXT(sub, entry); sub != NULL;
			    sub = TAILQ_NEXT(sub, entry)) {
				cmd_parse_print_break(buf, depth, oneline);
				cmd_parse_print_sequence(buf, sub, depth);
			}
			break;
		case CMD_PARSE_ELSE:
			cmd_parse_print_break(buf, depth, oneline);
			cmd_parse_strcat(buf, "%%else");
			TAILQ_FOREACH(sub, &child->children, entry) {
				cmd_parse_print_break(buf, depth, oneline);
				cmd_parse_print_sequence(buf, sub, depth);
			}
			break;
		default:
			break;
		}
	}
	cmd_parse_print_break(buf, depth, oneline);
	cmd_parse_strcat(buf, "%%endif");
}

static void
cmd_parse_print_item(char **buf, struct cmd_parse_node *item, u_int depth,
    int oneline)
{
	switch (item->type) {
	case CMD_PARSE_COMMAND:
		cmd_parse_print_command(buf, item, depth);
		break;
	case CMD_PARSE_ASSIGN:
		cmd_parse_strcat(buf, "%s=", item->value);
		cmd_parse_print_string(buf, TAILQ_FIRST(&item->children));
		break;
	case CMD_PARSE_HIDDEN_ASSIGN:
		cmd_parse_strcat(buf, "%%hidden %s=", item->value);
		cmd_parse_print_string(buf, TAILQ_FIRST(&item->children));
		break;
	case CMD_PARSE_IF:
		cmd_parse_print_if(buf, item, depth, oneline);
		break;
	default:
		break;
	}
}

static void
cmd_parse_print_sequence(char **buf, struct cmd_parse_node *seq, u_int depth)
{
	struct cmd_parse_node	*child;
	int			 first = 1, oneline = 0;

	if (TAILQ_NEXT(TAILQ_FIRST(&seq->children), entry) != NULL) {
		/*
		 * If there is more than one item in a sequence, force
		 * everything on to one line.
		 */
		oneline = 1;
	}

	TAILQ_FOREACH(child, &seq->children, entry) {
		if (!first)
			cmd_parse_strcat(buf, " ; ");
		first = 0;
		cmd_parse_print_item(buf, child, depth, oneline);
	}
}

char *
cmd_parse_print(struct cmd_parse_tree *tree)
{
	struct cmd_parse_node	*root = tree->root, *child;
	char			*buf = NULL;
	int			 first = 1;

	TAILQ_FOREACH(child, &root->children, entry) {
		if (!first)
			cmd_parse_strcat(&buf, "\n");
		first = 0;
		cmd_parse_print_sequence(&buf, child, 0);
	}
	if (buf == NULL)
		buf = xstrdup("");
	return (buf);
}

static char *
cmd_parse_make_string(struct cmd_parse_node *node)
{
	struct cmd_parse_node	*child;
	char			*s = NULL, *new;

	if (node->type != CMD_PARSE_STRING)
		return (NULL);

	TAILQ_FOREACH(child, &node->children, entry) {
		if (child->type != CMD_PARSE_TEXT) {
			free(s);
			return (NULL);
		}
		if (s == NULL)
			s = xstrdup(child->value);
		else {
			xasprintf(&new, "%s%s", s, child->value);
			free(s);
			s = new;
		}
	}
	if (s == NULL)
		s = xstrdup("");
	return (s);
}

static int
cmd_parse_command_any_have(struct cmd_parse_tree *tree,
    struct cmd_parse_node *node, int flag)
{
	struct cmd_parse_node	*child;
	int			 flags = tree->flags;
	struct args_value	*values = NULL;
	struct cmd		*cmd;
	char			*cause = NULL;
	u_int			 count = 0;
	int			 found = 0;

	if (node->type != CMD_PARSE_COMMAND)
		return (0);

	TAILQ_FOREACH(child, &node->children, entry) {
		values = xreallocarray(values, count + 1, sizeof *values);
		memset(&values[count], 0, sizeof values[count]);

		switch (child->type) {
		case CMD_PARSE_STRING:
			values[count].type = ARGS_STRING;
			values[count].string = cmd_parse_make_string(child);
			if (values[count].string == NULL) {
				found = -1;
				goto out;
			}
			break;
		case CMD_PARSE_COMMANDS:
			values[count].type = ARGS_COMMANDS;
			values[count].cmd = cmd_parse_from_node(tree, child);
			break;
		default:
			fatalx("unexpected node type in command");
		}
		count++;
	}

	cmd = cmd_parse(values, count, tree->file, node->line, flags, &cause);
	if (cmd == NULL) {
		free(cause);
		found = -1;
	} else {
		if (cmd_get_entry(cmd)->flags & flag)
			found = 1;
		cmd_free(cmd);
	}

out:
	args_free_values(values, count);
	free(values);
	return (found);
}

int
cmd_parse_any_have(struct cmd_parse_tree *tree, int flag)
{
	struct cmd_parse_node	*seq, *node;
	int			 found = 0, r;

	TAILQ_FOREACH(seq, &tree->root->children, entry) {
		if (seq->type != CMD_PARSE_SEQUENCE)
			continue;

		TAILQ_FOREACH(node, &seq->children, entry) {
			if (node->type != CMD_PARSE_COMMAND)
				continue;
			r = cmd_parse_command_any_have(tree, node, flag);
			if (r < 0)
				return (0);
			if (r)
				found = 1;
		}
	}
	return (found);
}

static void printflike(1, 2)
yyerror(const char *fmt, ...)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_input	*pi = ps->input;
	va_list			 ap;
	char			*error;

	if (ps->error != NULL)
		return;

	va_start(ap, fmt);
	xvasprintf(&error, fmt, ap);
	va_end(ap);

	ps->error = cmd_parse_get_error(pi->file, pi->line, error);
	free(error);
}

static int
yylex_is_var(char ch, int first)
{
	if (ch == '=')
		return (0);
	if (first && isdigit((u_char)ch))
		return (0);
	return (isalnum((u_char)ch) || ch == '_');
}

static void
yylex_append(char **buf, size_t *len, const char *add, size_t addlen)
{
	if (addlen > SIZE_MAX - 1 || *len > SIZE_MAX - 1 - addlen)
		fatalx("buffer is too big");
	*buf = xrealloc(*buf, (*len) + 1 + addlen);
	memcpy((*buf) + *len, add, addlen);
	(*len) += addlen;
}

static void
yylex_append1(char **buf, size_t *len, char add)
{
	yylex_append(buf, len, &add, 1);
}

static int
yylex_getc1(void)
{
	struct cmd_parse_state	*ps = &parse_state;
	int			 ch;

	if (ps->f != NULL)
		ch = getc(ps->f);
	else {
		if (ps->off == ps->len)
			ch = EOF;
		else
			ch = ps->buf[ps->off++];
	}
	return (ch);
}

static void
yylex_ungetc(int ch)
{
	struct cmd_parse_state	*ps = &parse_state;

	if (ps->f != NULL)
		ungetc(ch, ps->f);
	else if (ps->off > 0 && ch != EOF)
		ps->off--;
}

static int
yylex_getc(void)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_input	*pi = ps->input;
	int			 ch;

	if (ps->escapes != 0) {
		ps->escapes--;
		return ('\\');
	}
	for (;;) {
		ch = yylex_getc1();
		if (ch == '\\') {
			ps->escapes++;
			continue;
		}
		if (ch == '\n' && (ps->escapes % 2) == 1) {
			pi->line++;
			ps->escapes--;
			continue;
		}

		if (ps->escapes != 0) {
			yylex_ungetc(ch);
			ps->escapes--;
			return ('\\');
		}
		return (ch);
	}
}

static char *
yylex_get_word(int ch)
{
	char	*buf;
	size_t	 len;

	len = 0;
	buf = xmalloc(1);

	do
		yylex_append1(&buf, &len, ch);
	while ((ch = yylex_getc()) != EOF && strchr(" \t\n", ch) == NULL);
	yylex_ungetc(ch);

	buf[len] = '\0';
	return (buf);
}

/* Append literal bytes to the pending text of a string being scanned. */
static void
cmd_parse_scan_add(struct cmd_parse_scan *scan, const char *add, size_t addlen)
{
	yylex_append(&scan->text, &scan->len, add, addlen);
}

static void
cmd_parse_scan_add1(struct cmd_parse_scan *scan, char add)
{
	yylex_append1(&scan->text, &scan->len, add);
}

/* Flush pending literal text into a text child. */
static void
cmd_parse_scan_flush(struct cmd_parse_scan *scan)
{
	struct cmd_parse_node	*child;

	if (scan->len == 0)
		return;

	child = cmd_parse_new_node(CMD_PARSE_TEXT, scan->node->line);
	child->value = xmalloc(scan->len + 1);
	memcpy(child->value, scan->text, scan->len);
	child->value[scan->len] = '\0';
	TAILQ_INSERT_TAIL(&scan->node->children, child, entry);

	scan->len = 0;
}

/* Flush pending text, then add an environment or tilde child. */
static void
cmd_parse_scan_part(struct cmd_parse_scan *scan, enum cmd_parse_node_type type,
    const char *value)
{
	struct cmd_parse_node	*child;

	cmd_parse_scan_flush(scan);

	child = cmd_parse_new_node(type, scan->node->line);
	child->value = xstrdup(value);
	TAILQ_INSERT_TAIL(&scan->node->children, child, entry);
}

static int
yylex_token_escape(struct cmd_parse_scan *scan)
{
	int	 ch, type, o2, o3, mlen;
	u_int	 size, i, tmp;
	char	 s[9], m[MB_LEN_MAX];

	ch = yylex_getc();

	if (ch >= '4' && ch <= '7') {
		yyerror("invalid octal escape");
		return (0);
	}
	if (ch >= '0' && ch <= '3') {
		o2 = yylex_getc();
		if (o2 >= '0' && o2 <= '7') {
			o3 = yylex_getc();
			if (o3 >= '0' && o3 <= '7') {
				ch = 64 * (ch - '0') +
				      8 * (o2 - '0') +
					  (o3 - '0');
				cmd_parse_scan_add1(scan, ch);
				return (1);
			}
		}
		yyerror("invalid octal escape");
		return (0);
	}

	switch (ch) {
	case EOF:
		return (0);
	case 'a':
		ch = '\a';
		break;
	case 'b':
		ch = '\b';
		break;
	case 'e':
		ch = '\033';
		break;
	case 'f':
		ch = '\f';
		break;
	case 's':
		ch = ' ';
		break;
	case 'v':
		ch = '\v';
		break;
	case 'r':
		ch = '\r';
		break;
	case 'n':
		ch = '\n';
		break;
	case 't':
		ch = '\t';
		break;
	case 'u':
		type = 'u';
		size = 4;
		goto unicode;
	case 'U':
		type = 'U';
		size = 8;
		goto unicode;
	}

	cmd_parse_scan_add1(scan, ch);
	return (1);

unicode:
	for (i = 0; i < size; i++) {
		ch = yylex_getc();
		if (ch == EOF || ch == '\n')
			return (0);
		if (!isxdigit((u_char)ch)) {
			yyerror("invalid \\%c argument", type);
			return (0);
		}
		s[i] = ch;
	}
	s[i] = '\0';

	if ((size == 4 && sscanf(s, "%4x", &tmp) != 1) ||
	    (size == 8 && sscanf(s, "%8x", &tmp) != 1)) {
		yyerror("invalid \\%c argument", type);
		return (0);
	}
	mlen = wctomb(m, tmp);
	if (mlen <= 0 || mlen > (int)sizeof m) {
		yyerror("invalid \\%c argument", type);
		return (0);
	}
	cmd_parse_scan_add(scan, m, mlen);
	return (1);
}

static int
yylex_token_variable(struct cmd_parse_scan *scan)
{
	int		 ch, brackets = 0;
	char		 name[1024];
	size_t		 namelen = 0;

	ch = yylex_getc();
	if (ch == EOF)
		return (0);
	if (ch == '{')
		brackets = 1;
	else {
		if (!yylex_is_var(ch, 1)) {
			cmd_parse_scan_add1(scan, '$');
			yylex_ungetc(ch);
			return (1);
		}
		name[namelen++] = ch;
	}

	for (;;) {
		ch = yylex_getc();
		if (brackets && ch == '}')
			break;
		if (ch == EOF || !yylex_is_var(ch, 0)) {
			if (!brackets) {
				yylex_ungetc(ch);
				break;
			}
			yyerror("invalid environment variable");
			return (0);
		}
		if (namelen == (sizeof name) - 2) {
			yyerror("environment variable is too long");
			return (0);
		}
		name[namelen++] = ch;
	}
	name[namelen] = '\0';

	cmd_parse_scan_part(scan, CMD_PARSE_ENVIRONMENT, name);
	return (1);
}

static int
yylex_token_tilde(struct cmd_parse_scan *scan)
{
	int		 ch;
	char		 name[1024];
	size_t		 namelen = 0;

	for (;;) {
		ch = yylex_getc();
		if (ch == EOF || strchr("/ \t\n\"'", ch) != NULL) {
			yylex_ungetc(ch);
			break;
		}
		if (namelen == (sizeof name) - 2) {
			yyerror("user name is too long");
			return (0);
		}
		name[namelen++] = ch;
	}
	name[namelen] = '\0';

	cmd_parse_scan_part(scan, CMD_PARSE_TILDE, name);
	return (1);
}

/*
 * Scan a single string token into a string node with ordered text, environment
 * and tilde children. No expansion is performed: quote removal, line
 * continuation, comments and backslash escapes are handled, but $VAR, ~ and
 * formats are recorded literally for the executor to expand later.
 */
static struct cmd_parse_node *
yylex_token(int ch)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_input	*pi = ps->input;
	struct cmd_parse_scan	 scan;
	int			 inname = 1;
	size_t			 namelen = 0;
	enum { START,
	       NONE,
	       DOUBLE_QUOTES,
	       SINGLE_QUOTES }	 state = NONE, last = START;

	memset(&scan, 0, sizeof scan);
	scan.text = xmalloc(1);
	scan.node = cmd_parse_new_node(CMD_PARSE_STRING, pi->line);

	for (;;) {
		/* EOF or \n are always the end of the token. */
		if (ch == EOF)
			break;
		if (state == NONE && ch == '\r') {
			ch = yylex_getc();
			if (ch != '\n') {
				yylex_ungetc(ch);
				ch = '\r';
			}
		}
		if (ch == '\n') {
			if (state == NONE)
				break;
			pi->line++;
		}

		/* Whitespace or ; or } ends a token unless inside quotes. */
		if (state == NONE && (ch == ' ' || ch == '\t'))
			break;
		if (state == NONE && (ch == ';' || ch == '}'))
			break;

		/*
		 * Spaces and comments inside quotes after \n are removed but
		 * the \n is left.
		 */
		if (ch == '\n' && state != NONE) {
			cmd_parse_scan_add1(&scan, '\n');
			while ((ch = yylex_getc()) == ' ' || ch == '\t')
				/* nothing */;
			if (ch != '#')
				continue;
			ch = yylex_getc();
			if (strchr(",#{}:", ch) != NULL) {
				yylex_ungetc(ch);
				ch = '#';
			} else {
				while ((ch = yylex_getc()) != '\n' && ch != EOF)
					/* nothing */;
			}
			continue;
		}

		/* \ ~ and $ are expanded except in single quotes. */
		if (ch == '\\' && state != SINGLE_QUOTES) {
			inname = 0;
			if (!yylex_token_escape(&scan))
				goto error;
			goto skip;
		}
		if (ch == '~' && last != state && state != SINGLE_QUOTES) {
			inname = 0;
			if (!yylex_token_tilde(&scan))
				goto error;
			goto skip;
		}
		if (ch == '$' && state != SINGLE_QUOTES) {
			inname = 0;
			if (!yylex_token_variable(&scan))
				goto error;
			goto skip;
		}
		if (ch == '}' && state == NONE)
			goto error;  /* unmatched (matched ones were handled) */

		/*
		 * An unquoted "NAME=" prefix is an assignment: the value after
		 * the '=' begins a fresh word, so a leading ~ there is a tilde.
		 */
		if (ch == '=' && state == NONE && inname && namelen > 0) {
			cmd_parse_scan_add1(&scan, '=');
			inname = 0;
			last = START;
			goto next;
		}

		/* ' and " starts or end quotes (and is consumed). */
		if (ch == '\'') {
			if (state == NONE) {
				inname = 0;
				state = SINGLE_QUOTES;
				goto next;
			}
			if (state == SINGLE_QUOTES) {
				state = NONE;
				goto next;
			}
		}
		if (ch == '"') {
			if (state == NONE) {
				inname = 0;
				state = DOUBLE_QUOTES;
				goto next;
			}
			if (state == DOUBLE_QUOTES) {
				state = NONE;
				goto next;
			}
		}

		/* Otherwise add the character to the buffer. */
		if (inname) {
			if (namelen == 0 ? !yylex_is_var(ch, 1) :
			    !yylex_is_var(ch, 0))
				inname = 0;
			else
				namelen++;
		}
		cmd_parse_scan_add1(&scan, ch);

	skip:
		last = state;

	next:
		ch = yylex_getc();
	}
	yylex_ungetc(ch);

	cmd_parse_scan_flush(&scan);
	free(scan.text);

	return (scan.node);

error:
	cmd_parse_free_node(scan.node);
	free(scan.text);
	return (NULL);
}

static char *
yylex_format(void)
{
	char	*buf;
	size_t	 len;
	int	 ch, brackets = 1;

	len = 0;
	buf = xmalloc(1);

	yylex_append(&buf, &len, "#{", 2);
	for (;;) {
		if ((ch = yylex_getc()) == EOF || ch == '\n')
			goto error;
		if (ch == '#') {
			if ((ch = yylex_getc()) == EOF || ch == '\n')
				goto error;
			if (ch == '{')
				brackets++;
			yylex_append1(&buf, &len, '#');
		} else if (ch == '}') {
			if (brackets != 0 && --brackets == 0) {
				yylex_append1(&buf, &len, ch);
				break;
			}
		}
		yylex_append1(&buf, &len, ch);
	}
	if (brackets != 0)
		goto error;

	buf[len] = '\0';
	return (buf);

error:
	free(buf);
	return (NULL);
}

static int
yylex(void)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_input	*pi = ps->input;
	struct cmd_parse_node	*token;
	struct cmd_parse_node	*first;
	char			*word, *cp;
	int			 ch, next, condition;

	if (ps->eol)
		pi->line++;
	ps->eol = 0;

	condition = ps->condition;
	ps->condition = 0;

	for (;;) {
		ch = yylex_getc();

		if (ch == EOF) {
			/*
			 * Ensure every file or string is terminated by a
			 * newline. This keeps the parser simpler and avoids
			 * having to add a newline to each string.
			 */
			if (ps->eof)
				break;
			ps->eof = 1;
			return ('\n');
		}

		if (ch == ' ' || ch == '\t') {
			/*
			 * Ignore whitespace.
			 */
			continue;
		}

		if (ch == '\r') {
			/*
			 * Treat \r\n as \n.
			 */
			ch = yylex_getc();
			if (ch != '\n') {
				yylex_ungetc(ch);
				ch = '\r';
			}
		}
		if (ch == '\n') {
			/*
			 * End of line. Update the line number.
			 */
			ps->eol = 1;
			return ('\n');
		}

		if (ch == ';' || ch == '{' || ch == '}') {
			/*
			 * A semicolon or { or } is itself.
			 */
			return (ch);
		}

		if (ch == '#') {
			/*
			 * #{ after a condition opens a format; anything else
			 * is a comment, ignore up to the end of the line.
			 */
			next = yylex_getc();
			if (condition && next == '{') {
				word = yylex_format();
				if (word == NULL)
					return (ERROR);
				yylval.node = cmd_parse_token_from_string(word);
				free(word);
				return (FORMAT);
			}
			while (next != '\n' && next != EOF)
				next = yylex_getc();
			if (next == '\n') {
				pi->line++;
				return ('\n');
			}
			continue;
		}

		if (ch == '%') {
			/*
			 * % is a condition unless it is all % or all numbers,
			 * then it is a token.
			 */
			word = yylex_get_word('%');
			for (cp = word; *cp != '\0'; cp++) {
				if (*cp != '%' && !isdigit((u_char)*cp))
					break;
			}
			if (*cp == '\0') {
				yylval.node = cmd_parse_token_from_string(word);
				free(word);
				return (TOKEN);
			}
			ps->condition = 1;
			if (strcmp(word, "%hidden") == 0) {
				free(word);
				return (HIDDEN);
			}
			if (strcmp(word, "%if") == 0) {
				free(word);
				return (IF);
			}
			if (strcmp(word, "%else") == 0) {
				free(word);
				return (ELSE);
			}
			if (strcmp(word, "%elif") == 0) {
				free(word);
				return (ELIF);
			}
			if (strcmp(word, "%endif") == 0) {
				free(word);
				return (ENDIF);
			}
			free(word);
			return (ERROR);
		}

		/*
		 * Otherwise this is a token.
		 */
		token = yylex_token(ch);
		if (token == NULL)
			return (ERROR);
		yylval.node = token;

		/*
		 * If the token begins with a literal "NAME=", where NAME is a
		 * valid variable name, it is an assignment.
		 */
		first = TAILQ_FIRST(&token->children);
		if (first != NULL && first->type == CMD_PARSE_TEXT &&
		    yylex_is_var(first->value[0], 1)) {
			for (cp = first->value + 1; *cp != '='; cp++) {
				if (*cp == '\0' || !yylex_is_var(*cp, 0))
					break;
			}
			if (*cp == '=')
				return (EQUALS);
		}
		return (TOKEN);
	}
	return (0);
}
