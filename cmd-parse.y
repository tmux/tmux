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
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "tmux.h"

static int			 yylex(void);
static int			 yyparse(void);
static int printflike(1,2)	 yyerror(const char *, ...);

static char			*yylex_token(int);
static char			*yylex_format(void);

struct cmd_parse_scope {
	int				 flag;
	TAILQ_ENTRY (cmd_parse_scope)	 entry;
};

struct cmd_parse_command {
	u_int				  line;

	int				  argc;
	char				**argv;

	TAILQ_ENTRY(cmd_parse_command)	  entry;
};
TAILQ_HEAD(cmd_parse_commands, cmd_parse_command);

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
	struct cmd_parse_commands	*commands;

	struct cmd_parse_scope		*scope;
	TAILQ_HEAD(, cmd_parse_scope)	 stack;
};
static struct cmd_parse_state parse_state;

static char	*cmd_parse_get_error(const char *, u_int, const char *);
static void	 cmd_parse_free_command(struct cmd_parse_command *);
static struct cmd_parse_commands *cmd_parse_new_commands(void);
static void	 cmd_parse_free_commands(struct cmd_parse_commands *);
static char	*cmd_parse_commands_to_string(struct cmd_parse_commands *);
static void	 cmd_parse_print_commands(struct cmd_parse_input *, u_int,
		     struct cmd_list *);

%}

%union
{
	char					 *token;
	struct {
		int				  argc;
		char				**argv;
	} arguments;
	int					  flag;
	struct {
		int				  flag;
		struct cmd_parse_commands	 *commands;
	} elif;
	struct cmd_parse_commands		 *commands;
	struct cmd_parse_command		 *command;
}

%token ERROR
%token HIDDEN
%token IF
%token ELSE
%token ELIF
%token ENDIF
%token <token> FORMAT TOKEN EQUALS

%type <token> argument expanded format
%type <arguments> arguments
%type <flag> if_open if_elif
%type <elif> elif elif1
%type <commands> argument_statements statements statement
%type <commands> commands condition condition1
%type <command> command

%%

lines		: /* empty */
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
			TAILQ_CONCAT($$, $2, entry);
			free($2);
		}

statement	: /* empty */
		{
			$$ = xmalloc (sizeof *$$);
			TAILQ_INIT($$);
		}
		| hidden_assignment
		{
			$$ = xmalloc (sizeof *$$);
			TAILQ_INIT($$);
		}
		| condition
		{
			struct cmd_parse_state	*ps = &parse_state;

			if (ps->scope == NULL || ps->scope->flag)
				$$ = $1;
			else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($1);
			}
		}
		| commands
		{
			struct cmd_parse_state	*ps = &parse_state;

			if (ps->scope == NULL || ps->scope->flag)
				$$ = $1;
			else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($1);
			}
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
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_input	*pi = ps->input;
			struct format_tree	*ft;
			struct client		*c = pi->c;
			struct cmd_find_state	*fsp;
			struct cmd_find_state	 fs;
			int			 flags = FORMAT_NOJOBS;

			if (cmd_find_valid_state(&pi->fs))
				fsp = &pi->fs;
			else {
				cmd_find_from_client(&fs, c, 0);
				fsp = &fs;
			}
			ft = format_create(NULL, pi->item, FORMAT_NONE, flags);
			format_defaults(ft, c, fsp->s, fsp->wl, fsp->wp);

			$$ = format_expand(ft, $1);
			format_free(ft);
			free($1);
		}

optional_assignment	: /* empty */
			| assignment

assignment	: EQUALS
		{
			struct cmd_parse_state	*ps = &parse_state;
			int			 flags = ps->input->flags;

			if ((~flags & CMD_PARSE_PARSEONLY) &&
			    (ps->scope == NULL || ps->scope->flag))
				environ_put(global_environ, $1, 0);
			free($1);
		}

hidden_assignment : HIDDEN EQUALS
		{
			struct cmd_parse_state	*ps = &parse_state;
			int			 flags = ps->input->flags;

			if ((~flags & CMD_PARSE_PARSEONLY) &&
			    (ps->scope == NULL || ps->scope->flag))
				environ_put(global_environ, $2, ENVIRON_HIDDEN);
			free($2);
		}

if_open		: IF expanded
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_scope	*scope;

			scope = xmalloc(sizeof *scope);
			$$ = scope->flag = format_true($2);
			free($2);

			if (ps->scope != NULL)
				TAILQ_INSERT_HEAD(&ps->stack, ps->scope, entry);
			ps->scope = scope;
		}

if_else		: ELSE
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_scope	*scope;

			scope = xmalloc(sizeof *scope);
			scope->flag = !ps->scope->flag;

			free(ps->scope);
			ps->scope = scope;
		}

if_elif		: ELIF expanded
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_scope	*scope;

			scope = xmalloc(sizeof *scope);
			$$ = scope->flag = format_true($2);
			free($2);

			free(ps->scope);
			ps->scope = scope;
		}

if_close	: ENDIF
		{
			struct cmd_parse_state	*ps = &parse_state;

			free(ps->scope);
			ps->scope = TAILQ_FIRST(&ps->stack);
			if (ps->scope != NULL)
				TAILQ_REMOVE(&ps->stack, ps->scope, entry);
		}

condition	: if_open '\n' statements if_close
		{
			if ($1)
				$$ = $3;
			else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($3);
			}
		}
		| if_open '\n' statements if_else '\n' statements if_close
		{
			if ($1) {
				$$ = $3;
				cmd_parse_free_commands($6);
			} else {
				$$ = $6;
				cmd_parse_free_commands($3);
			}
		}
		| if_open '\n' statements elif if_close
		{
			if ($1) {
				$$ = $3;
				cmd_parse_free_commands($4.commands);
			} else if ($4.flag) {
				$$ = $4.commands;
				cmd_parse_free_commands($3);
			} else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($3);
				cmd_parse_free_commands($4.commands);
			}
		}
		| if_open '\n' statements elif if_else '\n' statements if_close
		{
			if ($1) {
				$$ = $3;
				cmd_parse_free_commands($4.commands);
				cmd_parse_free_commands($7);
			} else if ($4.flag) {
				$$ = $4.commands;
				cmd_parse_free_commands($3);
				cmd_parse_free_commands($7);
			} else {
				$$ = $7;
				cmd_parse_free_commands($3);
				cmd_parse_free_commands($4.commands);
			}
		}

elif		: if_elif '\n' statements
		{
			if ($1) {
				$$.flag = 1;
				$$.commands = $3;
			} else {
				$$.flag = 0;
				$$.commands = cmd_parse_new_commands();
				cmd_parse_free_commands($3);
			}
		}
		| if_elif '\n' statements elif
		{
			if ($1) {
				$$.flag = 1;
				$$.commands = $3;
				cmd_parse_free_commands($4.commands);
			} else if ($4.flag) {
				$$.flag = 1;
				$$.commands = $4.commands;
				cmd_parse_free_commands($3);
			} else {
				$$.flag = 0;
				$$.commands = cmd_parse_new_commands();
				cmd_parse_free_commands($3);
				cmd_parse_free_commands($4.commands);
			}
		}

commands	: command
		{
			struct cmd_parse_state	*ps = &parse_state;

			$$ = cmd_parse_new_commands();
			if ($1->argc != 0 &&
			    (ps->scope == NULL || ps->scope->flag))
				TAILQ_INSERT_TAIL($$, $1, entry);
			else
				cmd_parse_free_command($1);
		}
		| commands ';'
		{
			$$ = $1;
		}
		| commands ';' condition1
		{
			$$ = $1;
			TAILQ_CONCAT($$, $3, entry);
			free($3);
		}
		| commands ';' command
		{
			struct cmd_parse_state	*ps = &parse_state;

			if ($3->argc != 0 &&
			    (ps->scope == NULL || ps->scope->flag)) {
				$$ = $1;
				TAILQ_INSERT_TAIL($$, $3, entry);
			} else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($1);
				cmd_parse_free_command($3);
			}
		}
		| condition1
		{
			$$ = $1;
		}

command		: assignment
		{
			struct cmd_parse_state	*ps = &parse_state;

			$$ = xcalloc(1, sizeof *$$);
			$$->line = ps->input->line;
		}
		| optional_assignment TOKEN
		{
			struct cmd_parse_state	*ps = &parse_state;

			$$ = xcalloc(1, sizeof *$$);
			$$->line = ps->input->line;

			cmd_prepend_argv(&$$->argc, &$$->argv, $2);

		}
		| optional_assignment TOKEN arguments
		{
			struct cmd_parse_state	*ps = &parse_state;

			$$ = xcalloc(1, sizeof *$$);
			$$->line = ps->input->line;

			$$->argc = $3.argc;
			$$->argv = $3.argv;
			cmd_prepend_argv(&$$->argc, &$$->argv, $2);
		}

condition1	: if_open commands if_close
		{
			if ($1)
				$$ = $2;
			else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($2);
			}
		}
		| if_open commands if_else commands if_close
		{
			if ($1) {
				$$ = $2;
				cmd_parse_free_commands($4);
			} else {
				$$ = $4;
				cmd_parse_free_commands($2);
			}
		}
		| if_open commands elif1 if_close
		{
			if ($1) {
				$$ = $2;
				cmd_parse_free_commands($3.commands);
			} else if ($3.flag) {
				$$ = $3.commands;
				cmd_parse_free_commands($2);
			} else {
				$$ = cmd_parse_new_commands();
				cmd_parse_free_commands($2);
				cmd_parse_free_commands($3.commands);
			}
		}
		| if_open commands elif1 if_else commands if_close
		{
			if ($1) {
				$$ = $2;
				cmd_parse_free_commands($3.commands);
				cmd_parse_free_commands($5);
			} else if ($3.flag) {
				$$ = $3.commands;
				cmd_parse_free_commands($2);
				cmd_parse_free_commands($5);
			} else {
				$$ = $5;
				cmd_parse_free_commands($2);
				cmd_parse_free_commands($3.commands);
			}
		}

elif1		: if_elif commands
		{
			if ($1) {
				$$.flag = 1;
				$$.commands = $2;
			} else {
				$$.flag = 0;
				$$.commands = cmd_parse_new_commands();
				cmd_parse_free_commands($2);
			}
		}
		| if_elif commands elif1
		{
			if ($1) {
				$$.flag = 1;
				$$.commands = $2;
				cmd_parse_free_commands($3.commands);
			} else if ($3.flag) {
				$$.flag = 1;
				$$.commands = $3.commands;
				cmd_parse_free_commands($2);
			} else {
				$$.flag = 0;
				$$.commands = cmd_parse_new_commands();
				cmd_parse_free_commands($2);
				cmd_parse_free_commands($3.commands);
			}
		}

arguments	: argument
		{
			$$.argc = 1;
			$$.argv = xreallocarray(NULL, 1, sizeof *$$.argv);

			$$.argv[0] = $1;
		}
		| argument arguments
		{
			cmd_prepend_argv(&$2.argc, &$2.argv, $1);
			free($1);
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
			$$ = cmd_parse_commands_to_string($2);
			cmd_parse_free_commands($2);
		}

argument_statements	: statement '}'
			{
				$$ = $1;
			}
			| statements statement '}'
			{
				$$ = $1;
				TAILQ_CONCAT($$, $2, entry);
				free($2);
			}

%%

static char *
cmd_parse_get_error(const char *file, u_int line, const char *error)
{
	char	*s;

	if (file == NULL)
		s = xstrdup(error);
	else
		xasprintf (&s, "%s:%u: %s", file, line, error);
	return (s);
}

static void
cmd_parse_print_commands(struct cmd_parse_input *pi, u_int line,
    struct cmd_list *cmdlist)
{
	char	*s;

	if (pi->item != NULL && (pi->flags & CMD_PARSE_VERBOSE)) {
		s = cmd_list_print(cmdlist, 0);
		if (pi->file != NULL)
			cmdq_print(pi->item, "%s:%u: %s", pi->file, line, s);
		else
			cmdq_print(pi->item, "%u: %s", line, s);
		free(s);
	}
}

static void
cmd_parse_free_command(struct cmd_parse_command *cmd)
{
	cmd_free_argv(cmd->argc, cmd->argv);
	free(cmd);
}

static struct cmd_parse_commands *
cmd_parse_new_commands(void)
{
	struct cmd_parse_commands	*cmds;

	cmds = xmalloc(sizeof *cmds);
	TAILQ_INIT (cmds);
	return (cmds);
}

static void
cmd_parse_free_commands(struct cmd_parse_commands *cmds)
{
	struct cmd_parse_command	*cmd, *cmd1;

	TAILQ_FOREACH_SAFE(cmd, cmds, entry, cmd1) {
		TAILQ_REMOVE(cmds, cmd, entry);
		cmd_parse_free_command(cmd);
	}
	free(cmds);
}

static char *
cmd_parse_commands_to_string(struct cmd_parse_commands *cmds)
{
	struct cmd_parse_command	 *cmd;
	char				 *string = NULL, *s, *line;

	TAILQ_FOREACH(cmd, cmds, entry) {
		line = cmd_stringify_argv(cmd->argc, cmd->argv);
		if (string == NULL)
			s = line;
		else {
			xasprintf(&s, "%s ; %s", s, line);
			free(line);
		}

		free(string);
		string = s;
	}
	if (string == NULL)
		string = xstrdup("");
	log_debug("%s: %s", __func__, string);
	return (string);
}

static struct cmd_parse_commands *
cmd_parse_run_parser(char **cause)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_scope	*scope, *scope1;
	int			 retval;

	ps->commands = NULL;
	TAILQ_INIT(&ps->stack);

	retval = yyparse();
	TAILQ_FOREACH_SAFE(scope, &ps->stack, entry, scope1) {
		TAILQ_REMOVE(&ps->stack, scope, entry);
		free(scope);
	}
	if (retval != 0) {
		*cause = ps->error;
		return (NULL);
	}

	if (ps->commands == NULL)
		return (cmd_parse_new_commands());
	return (ps->commands);
}

static struct cmd_parse_commands *
cmd_parse_do_file(FILE *f, struct cmd_parse_input *pi, char **cause)
{
	struct cmd_parse_state	*ps = &parse_state;

	memset(ps, 0, sizeof *ps);
	ps->input = pi;
	ps->f = f;
	return (cmd_parse_run_parser(cause));
}

static struct cmd_parse_commands *
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

static struct cmd_parse_result *
cmd_parse_build_commands(struct cmd_parse_commands *cmds,
    struct cmd_parse_input *pi)
{
	static struct cmd_parse_result	 pr;
	struct cmd_parse_commands	*cmds2;
	struct cmd_parse_command	*cmd, *cmd2, *next, *next2, *after;
	u_int				 line = UINT_MAX;
	int				 i;
	struct cmd_list			*cmdlist = NULL, *result;
	struct cmd			*add;
	char				*name, *alias, *cause, *s;

	/* Check for an empty list. */
	if (TAILQ_EMPTY(cmds)) {
		cmd_parse_free_commands(cmds);
		pr.status = CMD_PARSE_EMPTY;
		return (&pr);
	}

	/*
	 * Walk the commands and expand any aliases. Each alias is parsed
	 * individually to a new command list, any trailing arguments appended
	 * to the last command, and all commands inserted into the original
	 * command list.
	 */
	TAILQ_FOREACH_SAFE(cmd, cmds, entry, next) {
		name = cmd->argv[0];

		alias = cmd_get_alias(name);
		if (alias == NULL)
			continue;

		line = cmd->line;
		log_debug("%s: %u %s = %s", __func__, line, name, alias);

		pi->line = line;
		cmds2 = cmd_parse_do_buffer(alias, strlen(alias), pi, &cause);
		free(alias);
		if (cmds2 == NULL) {
			pr.status = CMD_PARSE_ERROR;
			pr.error = cause;
			goto out;
		}

		cmd2 = TAILQ_LAST(cmds2, cmd_parse_commands);
		if (cmd2 == NULL) {
			TAILQ_REMOVE(cmds, cmd, entry);
			cmd_parse_free_command(cmd);
			continue;
		}
		for (i = 1; i < cmd->argc; i++)
			cmd_append_argv(&cmd2->argc, &cmd2->argv, cmd->argv[i]);

		after = cmd;
		TAILQ_FOREACH_SAFE(cmd2, cmds2, entry, next2) {
			cmd2->line = line;
			TAILQ_REMOVE(cmds2, cmd2, entry);
			TAILQ_INSERT_AFTER(cmds, after, cmd2, entry);
			after = cmd2;
		}
		cmd_parse_free_commands(cmds2);

		TAILQ_REMOVE(cmds, cmd, entry);
		cmd_parse_free_command(cmd);
	}

	/*
	 * Parse each command into a command list. Create a new command list
	 * for each line (unless the flag is set) so they get a new group (so
	 * the queue knows which ones to remove if a command fails when
	 * executed).
	 */
	result = cmd_list_new();
	TAILQ_FOREACH(cmd, cmds, entry) {
		name = cmd->argv[0];
		log_debug("%s: %u %s", __func__, cmd->line, name);
		cmd_log_argv(cmd->argc, cmd->argv, __func__);

		if (cmdlist == NULL ||
		    ((~pi->flags & CMD_PARSE_ONEGROUP) && cmd->line != line)) {
			if (cmdlist != NULL) {
				cmd_parse_print_commands(pi, line, cmdlist);
				cmd_list_move(result, cmdlist);
				cmd_list_free(cmdlist);
			}
			cmdlist = cmd_list_new();
		}
		line = cmd->line;

		add = cmd_parse(cmd->argc, cmd->argv, pi->file, line, &cause);
		if (add == NULL) {
			cmd_list_free(result);
			pr.status = CMD_PARSE_ERROR;
			pr.error = cmd_parse_get_error(pi->file, line, cause);
			free(cause);
			cmd_list_free(cmdlist);
			goto out;
		}
		cmd_list_append(cmdlist, add);
	}
	if (cmdlist != NULL) {
		cmd_parse_print_commands(pi, line, cmdlist);
		cmd_list_move(result, cmdlist);
		cmd_list_free(cmdlist);
	}

	s = cmd_list_print(result, 0);
	log_debug("%s: %s", __func__, s);
	free(s);

	pr.status = CMD_PARSE_SUCCESS;
	pr.cmdlist = result;

out:
	cmd_parse_free_commands(cmds);

	return (&pr);
}

struct cmd_parse_result *
cmd_parse_from_file(FILE *f, struct cmd_parse_input *pi)
{
	static struct cmd_parse_result	 pr;
	struct cmd_parse_input		 input;
	struct cmd_parse_commands	*cmds;
	char				*cause;

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}
	memset(&pr, 0, sizeof pr);

	cmds = cmd_parse_do_file(f, pi, &cause);
	if (cmds == NULL) {
		pr.status = CMD_PARSE_ERROR;
		pr.error = cause;
		return (&pr);
	}
	return (cmd_parse_build_commands(cmds, pi));
}

struct cmd_parse_result *
cmd_parse_from_string(const char *s, struct cmd_parse_input *pi)
{
	struct cmd_parse_input	input;

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}

	/*
	 * When parsing a string, put commands in one group even if there are
	 * multiple lines. This means { a \n b } is identical to "a ; b" when
	 * given as an argument to another command.
	 */
	pi->flags |= CMD_PARSE_ONEGROUP;
	return (cmd_parse_from_buffer(s, strlen(s), pi));
}

enum cmd_parse_status
cmd_parse_and_insert(const char *s, struct cmd_parse_input *pi,
    struct cmdq_item *after, struct cmdq_state *state, char **error)
{
	struct cmd_parse_result	*pr;
	struct cmdq_item	*item;

	pr = cmd_parse_from_string(s, pi);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		break;
	case CMD_PARSE_ERROR:
		if (error != NULL)
			*error = pr->error;
		else
			free(pr->error);
		break;
	case CMD_PARSE_SUCCESS:
		item = cmdq_get_command(pr->cmdlist, state);
		cmdq_insert_after(after, item);
		cmd_list_free(pr->cmdlist);
		break;
	}
	return (pr->status);
}

enum cmd_parse_status
cmd_parse_and_append(const char *s, struct cmd_parse_input *pi,
    struct client *c, struct cmdq_state *state, char **error)
{
	struct cmd_parse_result	*pr;
	struct cmdq_item	*item;

	pr = cmd_parse_from_string(s, pi);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		break;
	case CMD_PARSE_ERROR:
		if (error != NULL)
			*error = pr->error;
		else
			free(pr->error);
		break;
	case CMD_PARSE_SUCCESS:
		item = cmdq_get_command(pr->cmdlist, state);
		cmdq_append(c, item);
		cmd_list_free(pr->cmdlist);
		break;
	}
	return (pr->status);
}

struct cmd_parse_result *
cmd_parse_from_buffer(const void *buf, size_t len, struct cmd_parse_input *pi)
{
	static struct cmd_parse_result	 pr;
	struct cmd_parse_input		 input;
	struct cmd_parse_commands	*cmds;
	char				*cause;

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}
	memset(&pr, 0, sizeof pr);

	if (len == 0) {
		pr.status = CMD_PARSE_EMPTY;
		pr.cmdlist = NULL;
		pr.error = NULL;
		return (&pr);
	}

	cmds = cmd_parse_do_buffer(buf, len, pi, &cause);
	if (cmds == NULL) {
		pr.status = CMD_PARSE_ERROR;
		pr.error = cause;
		return (&pr);
	}
	return (cmd_parse_build_commands(cmds, pi));
}

struct cmd_parse_result *
cmd_parse_from_arguments(int argc, char **argv, struct cmd_parse_input *pi)
{
	struct cmd_parse_input		  input;
	struct cmd_parse_commands	 *cmds;
	struct cmd_parse_command	 *cmd;
	char				**copy, **new_argv;
	size_t				  size;
	int				  i, last, new_argc;

	/*
	 * The commands are already split up into arguments, so just separate
	 * into a set of commands by ';'.
	 */

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}
	cmd_log_argv(argc, argv, "%s", __func__);

	cmds = cmd_parse_new_commands();
	copy = cmd_copy_argv(argc, argv);

	last = 0;
	for (i = 0; i < argc; i++) {
		size = strlen(copy[i]);
		if (size == 0 || copy[i][size - 1] != ';')
			continue;
		copy[i][--size] = '\0';
		if (size > 0 && copy[i][size - 1] == '\\') {
			copy[i][size - 1] = ';';
			continue;
		}

		new_argc = i - last;
		new_argv = copy + last;
		if (size != 0)
			new_argc++;

		if (new_argc != 0) {
			cmd_log_argv(new_argc, new_argv, "%s: at %u", __func__,
			    i);

			cmd = xcalloc(1, sizeof *cmd);
			cmd->line = pi->line;

			cmd->argc = new_argc;
			cmd->argv = cmd_copy_argv(new_argc, new_argv);

			TAILQ_INSERT_TAIL(cmds, cmd, entry);
		}

		last = i + 1;
	}
	if (last != argc) {
		new_argv = copy + last;
		new_argc = argc - last;

		if (new_argc != 0) {
			cmd_log_argv(new_argc, new_argv, "%s: at %u", __func__,
			    last);

			cmd = xcalloc(1, sizeof *cmd);
			cmd->line = pi->line;

			cmd->argc = new_argc;
			cmd->argv = cmd_copy_argv(new_argc, new_argv);

			TAILQ_INSERT_TAIL(cmds, cmd, entry);
		}
	}

	cmd_free_argv(argc, copy);
	return (cmd_parse_build_commands(cmds, pi));
}

static int printflike(1, 2)
yyerror(const char *fmt, ...)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct cmd_parse_input	*pi = ps->input;
	va_list			 ap;
	char			*error;

	if (ps->error != NULL)
		return (0);

	va_start(ap, fmt);
	xvasprintf(&error, fmt, ap);
	va_end(ap);

	ps->error = cmd_parse_get_error(pi->file, pi->line, error);
	free(error);
	return (0);
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
			ps->input->line++;
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
	log_debug("%s: %s", __func__, buf);
	return (buf);
}

static int
yylex(void)
{
	struct cmd_parse_state	*ps = &parse_state;
	char			*token, *cp;
	int			 ch, next, condition;

	if (ps->eol)
		ps->input->line++;
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
				yylval.token = yylex_format();
				if (yylval.token == NULL)
					return (ERROR);
				return (FORMAT);
			}
			while (next != '\n' && next != EOF)
				next = yylex_getc();
			if (next == '\n') {
				ps->input->line++;
				return ('\n');
			}
			continue;
		}

		if (ch == '%') {
			/*
			 * % is a condition unless it is all % or all numbers,
			 * then it is a token.
			 */
			yylval.token = yylex_get_word('%');
			for (cp = yylval.token; *cp != '\0'; cp++) {
				if (*cp != '%' && !isdigit((u_char)*cp))
					break;
			}
			if (*cp == '\0')
				return (TOKEN);
			ps->condition = 1;
			if (strcmp(yylval.token, "%hidden") == 0) {
				free(yylval.token);
				return (HIDDEN);
			}
			if (strcmp(yylval.token, "%if") == 0) {
				free(yylval.token);
				return (IF);
			}
			if (strcmp(yylval.token, "%else") == 0) {
				free(yylval.token);
				return (ELSE);
			}
			if (strcmp(yylval.token, "%elif") == 0) {
				free(yylval.token);
				return (ELIF);
			}
			if (strcmp(yylval.token, "%endif") == 0) {
				free(yylval.token);
				return (ENDIF);
			}
			free(yylval.token);
			return (ERROR);
		}

		/*
		 * Otherwise this is a token.
		 */
		token = yylex_token(ch);
		if (token == NULL)
			return (ERROR);
		yylval.token = token;

		if (strchr(token, '=') != NULL && yylex_is_var(*token, 1)) {
			for (cp = token + 1; *cp != '='; cp++) {
				if (!yylex_is_var(*cp, 0))
					break;
			}
			if (*cp == '=')
				return (EQUALS);
		}
		return (TOKEN);
	}
	return (0);
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
	log_debug("%s: %s", __func__, buf);
	return (buf);

error:
	free(buf);
	return (NULL);
}

static int
yylex_token_escape(char **buf, size_t *len)
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
				yylex_append1(buf, len, ch);
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

	yylex_append1(buf, len, ch);
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
	yylex_append(buf, len, m, mlen);
	return (1);
}

static int
yylex_token_variable(char **buf, size_t *len)
{
	struct environ_entry	*envent;
	int			 ch, brackets = 0;
	char			 name[1024];
	size_t			 namelen = 0;
	const char		*value;

	ch = yylex_getc();
	if (ch == EOF)
		return (0);
	if (ch == '{')
		brackets = 1;
	else {
		if (!yylex_is_var(ch, 1)) {
			yylex_append1(buf, len, '$');
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

	envent = environ_find(global_environ, name);
	if (envent != NULL && envent->value != NULL) {
		value = envent->value;
		log_debug("%s: %s -> %s", __func__, name, value);
		yylex_append(buf, len, value, strlen(value));
	}
	return (1);
}

static int
yylex_token_tilde(char **buf, size_t *len)
{
	struct environ_entry	*envent;
	int			 ch;
	char			 name[1024];
	size_t			 namelen = 0;
	struct passwd		*pw;
	const char		*home = NULL;

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

	if (*name == '\0') {
		envent = environ_find(global_environ, "HOME");
		if (envent != NULL && *envent->value != '\0')
			home = envent->value;
		else if ((pw = getpwuid(getuid())) != NULL)
			home = pw->pw_dir;
	} else {
		if ((pw = getpwnam(name)) != NULL)
			home = pw->pw_dir;
	}
	if (home == NULL)
		return (0);

	log_debug("%s: ~%s -> %s", __func__, name, home);
	yylex_append(buf, len, home, strlen(home));
	return (1);
}

static char *
yylex_token(int ch)
{
	char			*buf;
	size_t			 len;
	enum { START,
	       NONE,
	       DOUBLE_QUOTES,
	       SINGLE_QUOTES }	 state = NONE, last = START;

	len = 0;
	buf = xmalloc(1);

	for (;;) {
		/* EOF or \n are always the end of the token. */
		if (ch == EOF || (state == NONE && ch == '\n'))
			break;

		/* Whitespace or ; or } ends a token unless inside quotes. */
		if ((ch == ' ' || ch == '\t' || ch == ';' || ch == '}') &&
		    state == NONE)
			break;

		/*
		 * Spaces and comments inside quotes after \n are removed but
		 * the \n is left.
		 */
		if (ch == '\n' && state != NONE) {
			yylex_append1(&buf, &len, '\n');
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
			if (!yylex_token_escape(&buf, &len))
				goto error;
			goto skip;
		}
		if (ch == '~' && last != state && state != SINGLE_QUOTES) {
			if (!yylex_token_tilde(&buf, &len))
				goto error;
			goto skip;
		}
		if (ch == '$' && state != SINGLE_QUOTES) {
			if (!yylex_token_variable(&buf, &len))
				goto error;
			goto skip;
		}
		if (ch == '}' && state == NONE)
			goto error;  /* unmatched (matched ones were handled) */

		/* ' and " starts or end quotes (and is consumed). */
		if (ch == '\'') {
			if (state == NONE) {
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
				state = DOUBLE_QUOTES;
				goto next;
			}
			if (state == DOUBLE_QUOTES) {
				state = NONE;
				goto next;
			}
		}

		/* Otherwise add the character to the buffer. */
		yylex_append1(&buf, &len, ch);

	skip:
		last = state;

	next:
		ch = yylex_getc();
	}
	yylex_ungetc(ch);

	buf[len] = '\0';
	log_debug("%s: %s", __func__, buf);
	return (buf);

error:
	free(buf);
	return (NULL);
}
