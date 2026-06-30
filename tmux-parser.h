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
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef TMUX_PARSER_H
#define TMUX_PARSER_H

#include <sys/types.h>

#include <stdio.h>

/*
 * Parsed command tree.
 *
 * The parser builds syntax only. It does not expand formats, environment
 * variables, or ~, does not evaluate %if/%elif, does not expand aliases, and
 * does not build struct cmd or struct cmd_list. Those are execution-time
 * operations.
 *
 * Command failure scope is represented by CMD_PARSE_SEQUENCE: if an invoked
 * command or assignment fails, the invoker skips the remaining children of
 * that sequence. No explicit group ID is stored in the tree.
 */

struct cmd_parse_tree;
struct cmd_parse_node;
struct args_value;

struct cmd_parse_input {
	int			 flags;
#define CMD_PARSE_QUIET 0x1
#define CMD_PARSE_PARSEONLY 0x2 /* XXX */
#define CMD_PARSE_NOALIAS 0x4 /* XXX */
#define CMD_PARSE_VERBOSE 0x8 /* XXX */
#define CMD_PARSE_ONEGROUP 0x10

	const char		*file;
	u_int			 line;
};

enum cmd_parse_node_type {
	CMD_PARSE_ROOT,
	CMD_PARSE_SEQUENCE,

	CMD_PARSE_COMMAND,
	CMD_PARSE_STRING,
	CMD_PARSE_COMMANDS,

	CMD_PARSE_TEXT,
	CMD_PARSE_ENVIRONMENT,
	CMD_PARSE_TILDE,

	CMD_PARSE_ASSIGN,
	CMD_PARSE_HIDDEN_ASSIGN,

	CMD_PARSE_IF,
	CMD_PARSE_ELIF,
	CMD_PARSE_ELSE
};

struct cmd_parse_tree	*cmd_parse_from_file(FILE *, struct cmd_parse_input *,
			    char **);
struct cmd_parse_tree	*cmd_parse_from_buffer(const void *, size_t,
			    struct cmd_parse_input *, char **);
struct cmd_parse_tree	*cmd_parse_from_string(const char *,
			    struct cmd_parse_input *, char **);
struct cmd_parse_tree	*cmd_parse_from_node(struct cmd_parse_node *);
struct cmd_parse_tree	*cmd_parse_from_arguments(struct args_value *, u_int);
struct cmd_parse_tree	*cmd_parse_add_ref(struct cmd_parse_tree *);
void			 cmd_parse_free(struct cmd_parse_tree *);
struct cmd_parse_node	*cmd_parse_root(struct cmd_parse_tree *);
char			*cmd_parse_print(struct cmd_parse_tree *);
void			 cmd_parse_log(const char *, struct cmd_parse_tree *);
void			 cmd_parse_log_node(const char *,
			     struct cmd_parse_node *);
enum cmd_parse_node_type cmd_parse_node_type(struct cmd_parse_node *);
const char		*cmd_parse_node_type_string(enum cmd_parse_node_type);
const char		*cmd_parse_node_value(struct cmd_parse_node *);
u_int			 cmd_parse_node_line(struct cmd_parse_node *);
u_int			 cmd_parse_node_end_line(struct cmd_parse_node *);
struct cmd_parse_node	*cmd_parse_node_first_child(struct cmd_parse_node *);
struct cmd_parse_node	*cmd_parse_node_next(struct cmd_parse_node *);

#endif /* TMUX_PARSER_H */
