/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"
#include "tmux-parser.h"

/* One frame in the parse tree stack. */
struct cmd_invoke_frame {
	struct cmd_parse_node	*node;
	struct cmd_parse_node	*next;
	struct cmd_parse_node	*end;
};

/* Shared state for a parsed command tree invocation. */
struct cmd_invoke_state {
	u_int			 references;
	struct cmd_parse_tree	*tree;

	struct cmd_invoke_frame	*stack;
	u_int			 nstack;

	enum cmd_retval		 last;
	int			 have_last;

	int			 argc;
	char		       **argv;
};

static void	cmd_invoke_push(struct cmd_invoke_state *,
		    struct cmd_parse_node *, struct cmd_parse_node *,
		    struct cmd_parse_node *);
static struct cmd_parse_node *cmd_invoke_next(struct cmd_invoke_state *);
static void	cmd_invoke_skip_sequence(struct cmd_invoke_state *);
static int	cmd_invoke_expand_string(struct cmdq_item *,
		    struct cmd_invoke_state *, struct cmd_parse_node *,
		    char **);
static int	cmd_invoke_assignment(struct cmdq_item *,
		    struct cmd_invoke_state *, struct cmd_parse_node *);
static int	cmd_invoke_if(struct cmdq_item *, struct cmd_invoke_state *,
		    struct cmd_parse_node *);
static struct cmd *cmd_invoke_build_command(struct cmdq_item *,
		     struct cmd_invoke_state *, struct cmd_parse_node *);
static int	cmd_invoke_read_only(struct cmdq_item *, struct cmd *);

/* Push a node's child range onto the traversal stack. */
static void
cmd_invoke_push(struct cmd_invoke_state *is, struct cmd_parse_node *node,
    struct cmd_parse_node *first, struct cmd_parse_node *end)
{
	u_int	n = is->nstack + 1;

	is->stack = xreallocarray(is->stack, n, sizeof *is->stack);
	is->stack[is->nstack].node = node;
	is->stack[is->nstack].next = first;
	is->stack[is->nstack].end = end;
	is->nstack = n;
}

/* Return the next node to execute from the traversal stack. */
static struct cmd_parse_node *
cmd_invoke_next(struct cmd_invoke_state *is)
{
	struct cmd_invoke_frame	*frame;
	struct cmd_parse_node	*node;

	for (;;) {
		if (is->nstack == 0)
			return (NULL);
		frame = &is->stack[is->nstack - 1];
		if (frame->next != NULL && frame->next != frame->end)
			break;
		is->nstack--;
	}

	node = frame->next;
	frame->next = cmd_parse_node_next(node);
	return (node);
}

/*
 * Skip the rest of the active sequence after a command failure. Failure scope
 * is the active CMD_PARSE_SEQUENCE. Discard nested frames and advance only as
 * far as the end of that sequence.
 */
static void
cmd_invoke_skip_sequence(struct cmd_invoke_state *is)
{
	enum cmd_parse_node_type	type;
	u_int				i;

	for (i = is->nstack; i > 0; i--) {
		type = cmd_parse_node_type(is->stack[i - 1].node);
		if (type == CMD_PARSE_SEQUENCE) {
			is->stack[i - 1].next = is->stack[i - 1].end;
			is->nstack = i;
			break;
		}
	}
	is->have_last = 0;
}

/* Append a string to a dynamically allocated buffer. */
static void
cmd_invoke_append(char **buf, size_t *len, const char *s)
{
	size_t	slen;

	if (s != NULL) {
		slen = strlen(s);
		*buf = xrealloc(*buf, *len + slen + 1);
		memcpy(*buf + *len, s, slen + 1);
		*len += slen;
	}
}

/* Look up an environment variable. */
static const char *
cmd_invoke_getenv(struct cmdq_item *item, const char *name)
{
	struct client		*c = cmdq_get_client(item);
	struct environ_entry	*envent;

	if (c != NULL && c->environ != NULL) {
		envent = environ_find(c->environ, name);
		if (envent != NULL && envent->value != NULL)
			return (envent->value);
	}
	envent = environ_find(global_environ, name);
	if (envent != NULL && envent->value != NULL)
		return (envent->value);
	return ("");
}

/* Resolve a tilde expansion to a home directory. */
static const char *
cmd_invoke_tilde(const char *name)
{
	struct passwd	*pw;

	if (name == NULL || *name == '\0')
		return (find_home());
	if ((pw = getpwnam(name)) == NULL)
		return ("");
	return (pw->pw_dir);
}

/* Expand a parsed string node into an argv string. */
static int
cmd_invoke_expand_string(struct cmdq_item *item, struct cmd_invoke_state *is,
    struct cmd_parse_node *node, char **out)
{
	struct cmd_parse_node	*child;
	const char		*s, *value;
	char			*buf = NULL, *new;
	size_t			 len = 0;
	int			 i;

	child = cmd_parse_node_first_child(node);
	while (child != NULL) {
		value = cmd_parse_node_value(child);
		switch (cmd_parse_node_type(child)) {
		case CMD_PARSE_TEXT:
			s = value;
			break;
		case CMD_PARSE_ENVIRONMENT:
			s = cmd_invoke_getenv(item, value);
			break;
		case CMD_PARSE_TILDE:
			s = cmd_invoke_tilde(value);
			break;
		default:
			fatalx("unexpected node type in string");
		}
		cmd_invoke_append(&buf, &len, s);
		child = cmd_parse_node_next(child);
	}
	if (buf == NULL)
		buf = xstrdup("");
	for (i = 0; i < is->argc; i++) {
		new = cmd_template_replace(buf, is->argv[i], i + 1);
		free(buf);
		buf = new;
	}
	*out = buf;
	return (0);
}

/* Execute an assignment node by updating the environment. */
static int
cmd_invoke_assignment(struct cmdq_item *item, struct cmd_invoke_state *is,
    struct cmd_parse_node *node)
{
	struct cmd_parse_node	*value;
	const char		*name;
	char			*expanded;
	int			 flags = 0;

	value = cmd_parse_node_first_child(node);
	if (value == NULL)
		return (-1);
	if (cmd_invoke_expand_string(item, is, value, &expanded) != 0)
		return (-1);

	name = cmd_parse_node_value(node);
	if (cmd_parse_node_type(node) == CMD_PARSE_HIDDEN_ASSIGN)
		flags |= ENVIRON_HIDDEN;
	environ_set(global_environ, name, flags, "%s", expanded);
	free(expanded);
	return (0);
}

/* Expand and evaluate a conditional expression. */
static int
cmd_invoke_is_true(struct cmdq_item *item,
    struct cmd_invoke_state *is, struct cmd_parse_node *node, int *result)
{
	struct format_tree	*ft;
	char			*s, *expanded;

	if (cmd_invoke_expand_string(item, is, node, &s) != 0)
		return (-1);
	ft = format_create_from_target(item);

	expanded = format_expand(ft, s);
	*result = format_true(expanded);
	free(expanded);

	format_free(ft);
	free(s);
	return (0);
}

/* Skip else branches. */
static struct cmd_parse_node *
cmd_invoke_if_branch_end(struct cmd_parse_node *first)
{
	struct cmd_parse_node	*node;

	for (node = first; node != NULL; node = cmd_parse_node_next(node)) {
		switch (cmd_parse_node_type(node)) {
		case CMD_PARSE_ELIF:
		case CMD_PARSE_ELSE:
			return (node);
		default:
			break;
		}
	}
	return (NULL);
}

/* Select and queue the branch for a conditional node. */
static int
cmd_invoke_if(struct cmdq_item *item, struct cmd_invoke_state *is,
    struct cmd_parse_node *node)
{
	struct cmd_parse_node	*child, *first, *next;
	int			 r;

	next = cmd_parse_node_first_child(node);
	if (next == NULL)
		return (0);
	first = cmd_parse_node_next(next);
	if (cmd_invoke_is_true(item, is, next, &r) != 0)
		return (-1);
	if (r) {
		next = cmd_invoke_if_branch_end(first);
		cmd_invoke_push(is, node, first, next);
		return (0);
	}

	for (child = first; child != NULL; child = cmd_parse_node_next(child)) {
		switch (cmd_parse_node_type(child)) {
		case CMD_PARSE_ELIF:
			next = cmd_parse_node_first_child(child);
			if (next == NULL)
				break;
			if (cmd_invoke_is_true(item, is, next, &r) != 0)
				return (-1);
			if (r) {
				next = cmd_parse_node_next(next);
				cmd_invoke_push(is, child, next, NULL);
				return (0);
			}
			break;
		case CMD_PARSE_ELSE:
			next = cmd_parse_node_first_child(child);
			cmd_invoke_push(is, child, next, NULL);
			return (0);
		default:
			break;
		}
	}
	return (0);
}

/* Report an error from an invoke item. */
static void
cmd_invoke_error(struct cmdq_item *item, struct cmd_invoke_state *is,
    struct cmd_parse_node *node, const char *cause)
{
	const char	*file = cmd_parse_file(is->tree);
	u_int		 line = cmd_parse_node_line(node);

	if (cmdq_get_client(item) != NULL) {
		cmdq_error(item, "%s", cause);
		return;
	}

	if (file != NULL)
		cfg_add_cause("%s:%u: %s", file, line, cause);
	else
		cfg_add_cause("%s", cause);
}

/* Build one command from a parsed command node. */
static struct cmd *
cmd_invoke_build_command(struct cmdq_item *item, struct cmd_invoke_state *is,
    struct cmd_parse_node *node)
{
	struct cmd_parse_tree	*tree = is->tree;
	struct cmd_parse_node	*child;
	struct args_value	*values = NULL;
	struct cmd		*cmd;
	const char		*file = cmd_parse_file(is->tree);
	u_int			 line = cmd_parse_node_line(node);
	int			 flags = cmd_parse_flags(is->tree);
	char			*cause = NULL;
	u_int			 count = 0;

	child = cmd_parse_node_first_child(node);
	while (child != NULL) {
		values = xreallocarray(values, count + 1, sizeof *values);
		memset(&values[count], 0, sizeof values[count]);
		switch (cmd_parse_node_type(child)) {
		case CMD_PARSE_STRING:
			values[count].type = ARGS_STRING;
			if (cmd_invoke_expand_string(item, is, child,
			    &values[count].string) != 0)
				goto fail;
			break;
		case CMD_PARSE_COMMANDS:
			values[count].type = ARGS_COMMANDS;
			values[count].cmd = cmd_parse_from_node(tree, child);
			break;
		default:
			fatalx("unexpected node type in command");
		}
		count++;
		child = cmd_parse_node_next(child);
	}

	cmd = cmd_parse(values, count, file, line, flags, &cause);
	if (cmd == NULL) {
		cmd_invoke_error(item, is, node, cause);
		free(cause);
		goto fail;
	}
	args_free_values(values, count);
	free(values);
	return (cmd);

fail:
	args_free_values(values, count);
	free(values);
	return (NULL);
}

static int
cmd_invoke_read_only(struct cmdq_item *item, struct cmd *cmd)
{
	struct client		*c = cmdq_get_client(item);
	const struct cmd_entry	*entry = cmd_get_entry(cmd);

	if (c == NULL || (~c->flags & CLIENT_READONLY))
		return (0);
	if (entry->flags & CMD_READONLY)
		return (0);
	cmdq_error(item, "client is read-only");
	return (-1);
}

/* Create the first invoke queue item for a parsed tree. */
struct cmdq_item *
cmd_invoke_get(struct cmd_parse_tree *tree, struct cmdq_state *state,
    const struct cmd_invoke_input *input)
{
	struct cmd_invoke_state	*is;
	struct cmd_parse_node	*root = cmd_parse_root(tree), *first;
	struct cmdq_item	*item;
	struct cmd_invoke_input	 new_input = { 0 };
	int			 i;

	if (input == NULL)
		input = &new_input;

	is = xcalloc(1, sizeof *is);
	is->references = 1;
	is->tree = cmd_parse_add_ref(tree);

	is->argc = input->argc;
	if (input->argc != 0) {
		is->argv = xreallocarray(NULL, input->argc, sizeof *is->argv);
		for (i = 0; i < input->argc; i++)
			is->argv[i] = xstrdup(input->argv[i]);
	}

	first = cmd_parse_node_first_child(root);
	cmd_invoke_push(is, root, first, NULL);

	item = cmdq_get_invoke(is, state);
	cmd_invoke_state_free(is);
	return (item);
}

/* Add a reference to shared invoke state. */
struct cmd_invoke_state *
cmd_invoke_state_add_ref(struct cmd_invoke_state *is)
{
	is->references++;
	return (is);
}

/* Release a reference to shared invoke state. */
void
cmd_invoke_state_free(struct cmd_invoke_state *is)
{
	int	i;

	if (is == NULL)
		return;
	if (--is->references != 0)
		return;

	for (i = 0; i < is->argc; i++)
		free(is->argv[i]);
	free(is->argv);

	cmd_parse_free(is->tree);
	free(is->stack);
	free(is);
}

/* Record the result from the last command item. */
void
cmd_invoke_result(struct cmd_invoke_state *is, enum cmd_retval retval)
{
	is->last = retval;
	is->have_last = 1;
}

/* Fire an invoke item and queue the next command plus continuation. */
enum cmd_retval
cmd_invoke_fire(struct cmdq_item *item, struct cmd_invoke_state *is)
{
	struct cmd_parse_node	*node;
	struct cmdq_item	*new_item, *next;
	struct cmdq_state	*state;
	struct cmd		*cmd;

	if (is->have_last && is->last == CMD_RETURN_ERROR)
		cmd_invoke_skip_sequence(is);
	else
		is->have_last = 0;

	for (;;) {
		node = cmd_invoke_next(is);
		if (node == NULL)
			return (CMD_RETURN_NORMAL);
		cmd_parse_log_node(__func__, node);

		switch (cmd_parse_node_type(node)) {
		case CMD_PARSE_ROOT:
		case CMD_PARSE_SEQUENCE:
			cmd_invoke_push(is, node,
			    cmd_parse_node_first_child(node), NULL);
			break;
		case CMD_PARSE_ASSIGN:
		case CMD_PARSE_HIDDEN_ASSIGN:
			if (cmd_invoke_assignment(item, is, node) != 0) {
				cmdq_error(item, "bad assignment");
				cmd_invoke_skip_sequence(is);
			}
			break;
		case CMD_PARSE_IF:
			if (cmd_invoke_if(item, is, node) != 0) {
				cmdq_error(item, "bad condition");
				cmd_invoke_skip_sequence(is);
			}
			break;
		case CMD_PARSE_ELIF:
		case CMD_PARSE_ELSE:
			break;
		case CMD_PARSE_COMMAND:
			cmd = cmd_invoke_build_command(item, is, node);
			if (cmd == NULL) {
				cmd_invoke_skip_sequence(is);
				break;
			}
			if (cmd_invoke_read_only(item, cmd) != 0) {
				cmd_free(cmd);
				cmd_invoke_skip_sequence(is);
				break;
			}

			/*
			 * Queue one command followed by this walker. WAIT and
			 * command-inserted items therefore run before resume.
			 */
			state = cmdq_get_state(item);
			new_item = cmdq_get_one_command(cmd, state, is);
			next = cmdq_get_invoke(is, state);
			cmdq_insert_after(item, next);
			cmdq_insert_after(item, new_item);
			return (CMD_RETURN_NORMAL);
		case CMD_PARSE_STRING:
		case CMD_PARSE_COMMANDS:
		case CMD_PARSE_TEXT:
		case CMD_PARSE_ENVIRONMENT:
		case CMD_PARSE_TILDE:
			fatalx("unexpected node type");
		}
	}
}
