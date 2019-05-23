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
#include <string.h>
#include <unistd.h>

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
	char				 *name;
	u_int				  line;

	int				  argc;
	char				**argv;

	TAILQ_ENTRY(cmd_parse_command)	  entry;
};
TAILQ_HEAD(cmd_parse_commands, cmd_parse_command);

struct cmd_parse_state {
	FILE				*f;
	int				 eof;
	struct cmd_parse_input		*input;
	u_int				 escapes;

	char				*error;
	struct cmd_parse_commands	 commands;

	struct cmd_parse_scope		*scope;
	TAILQ_HEAD(, cmd_parse_scope)	 stack;
};
static struct cmd_parse_state parse_state;

static char	*cmd_parse_get_error(const char *, u_int, const char *);
static char	*cmd_parse_get_strerror(const char *, u_int);
static void	 cmd_parse_free_command(struct cmd_parse_command *);
static void	 cmd_parse_free_commands(struct cmd_parse_commands *);

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
		struct cmd_parse_commands	  commands;
	} elif;
	struct cmd_parse_commands		  commands;
	struct cmd_parse_command		 *command;
}

%token ERROR
%token IF
%token ELSE
%token ELIF
%token ENDIF
%token <token> FORMAT TOKEN EQUALS

%type <token> argument expanded
%type <arguments> arguments
%type <flag> if_open if_elif
%type <elif> elif elif1
%type <commands> statements statement commands condition condition1
%type <command> command

%%

lines		: /* empty */
		| statements
		{
			struct cmd_parse_state	*ps = &parse_state;

			TAILQ_CONCAT(&ps->commands, &$1, entry);
		}

statements	: statement '\n'
		{
			TAILQ_INIT(&$$);
			TAILQ_CONCAT(&$$, &$1, entry);
		}
		| statements statement '\n'
		{
			TAILQ_INIT(&$$);
			TAILQ_CONCAT(&$$, &$1, entry);
			TAILQ_CONCAT(&$$, &$2, entry);
		}


statement	: condition
		{
			struct cmd_parse_state	*ps = &parse_state;

			TAILQ_INIT(&$$);
			if (ps->scope == NULL || ps->scope->flag)
				TAILQ_CONCAT(&$$, &$1, entry);
			else
				cmd_parse_free_commands(&$1);
		}
		| assignment
		{
			TAILQ_INIT(&$$);
		}
		| commands
		{
			struct cmd_parse_state	*ps = &parse_state;

			TAILQ_INIT(&$$);
			if (ps->scope == NULL || ps->scope->flag)
				TAILQ_CONCAT(&$$, &$1, entry);
			else
				cmd_parse_free_commands(&$1);
		}

expanded	: FORMAT
		{
			struct cmd_parse_state	*ps = &parse_state;
			struct cmd_parse_input	*pi = ps->input;
			struct format_tree	*ft;
			struct client		*c = pi->c;
			struct cmd_find_state	*fs;
			int			 flags = FORMAT_NOJOBS;

			if (cmd_find_valid_state(&pi->fs))
				fs = &pi->fs;
			else
				fs = NULL;
			ft = format_create(NULL, pi->item, FORMAT_NONE, flags);
			if (fs != NULL)
				format_defaults(ft, c, fs->s, fs->wl, fs->wp);
			else
				format_defaults(ft, c, NULL, NULL, NULL);

			$$ = format_expand(ft, $1);
			format_free(ft);
			free($1);
		}

assignment	: /* empty */
		| EQUALS
		{
			struct cmd_parse_state	*ps = &parse_state;
			int			 flags = ps->input->flags;

			if ((~flags & CMD_PARSE_PARSEONLY) &&
			    (ps->scope == NULL || ps->scope->flag))
				environ_put(global_environ, $1);
			free($1);
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
			TAILQ_INIT(&$$);
			if ($1)
				TAILQ_CONCAT(&$$, &$3, entry);
			else
				cmd_parse_free_commands(&$3);
		}
		| if_open '\n' statements if_else '\n' statements if_close
		{
			TAILQ_INIT(&$$);
			if ($1) {
				TAILQ_CONCAT(&$$, &$3, entry);
				cmd_parse_free_commands(&$6);
			} else {
				TAILQ_CONCAT(&$$, &$6, entry);
				cmd_parse_free_commands(&$3);
			}
		}
		| if_open '\n' statements elif if_close
		{
			TAILQ_INIT(&$$);
			if ($1) {
				TAILQ_CONCAT(&$$, &$3, entry);
				cmd_parse_free_commands(&$4.commands);
			} else if ($4.flag) {
				TAILQ_CONCAT(&$$, &$4.commands, entry);
				cmd_parse_free_commands(&$3);
			} else {
				cmd_parse_free_commands(&$3);
				cmd_parse_free_commands(&$4.commands);
			}
		}
		| if_open '\n' statements elif if_else '\n' statements if_close
		{
			TAILQ_INIT(&$$);
			if ($1) {
				TAILQ_CONCAT(&$$, &$3, entry);
				cmd_parse_free_commands(&$4.commands);
				cmd_parse_free_commands(&$7);
			} else if ($4.flag) {
				TAILQ_CONCAT(&$$, &$4.commands, entry);
				cmd_parse_free_commands(&$3);
				cmd_parse_free_commands(&$7);
			} else {
				TAILQ_CONCAT(&$$, &$7, entry);
				cmd_parse_free_commands(&$3);
				cmd_parse_free_commands(&$4.commands);
			}
		}

elif		: if_elif '\n' statements
		{
			TAILQ_INIT(&$$.commands);
			if ($1)
				TAILQ_CONCAT(&$$.commands, &$3, entry);
			else
				cmd_parse_free_commands(&$3);
			$$.flag = $1;
		}
		| if_elif '\n' statements elif
		{
			TAILQ_INIT(&$$.commands);
			if ($1) {
				$$.flag = 1;
				TAILQ_CONCAT(&$$.commands, &$3, entry);
				cmd_parse_free_commands(&$4.commands);
			} else {
				$$.flag = $4.flag;
				TAILQ_CONCAT(&$$.commands, &$4.commands, entry);
				cmd_parse_free_commands(&$3);
			}
		}


commands	: command
		{
			struct cmd_parse_state	*ps = &parse_state;

			TAILQ_INIT(&$$);
			if (ps->scope == NULL || ps->scope->flag)
				TAILQ_INSERT_TAIL(&$$, $1, entry);
			else
				cmd_parse_free_command($1);
		}
		| commands ';'
		{
			TAILQ_INIT(&$$);
			TAILQ_CONCAT(&$$, &$1, entry);
		}
		| commands ';' condition1
		{
			TAILQ_INIT(&$$);
			TAILQ_CONCAT(&$$, &$1, entry);
			TAILQ_CONCAT(&$$, &$3, entry);
		}
		| commands ';' command
		{
			struct cmd_parse_state	*ps = &parse_state;

			TAILQ_INIT(&$$);
			if (ps->scope == NULL || ps->scope->flag) {
				TAILQ_CONCAT(&$$, &$1, entry);
				TAILQ_INSERT_TAIL(&$$, $3, entry);
			} else {
				cmd_parse_free_commands(&$1);
				cmd_parse_free_command($3);
			}
		}
		| condition1
		{
			TAILQ_INIT(&$$);
			TAILQ_CONCAT(&$$, &$1, entry);
		}

command		: assignment TOKEN
		{
			struct cmd_parse_state	*ps = &parse_state;

			$$ = xcalloc(1, sizeof *$$);
			$$->name = $2;
			$$->line = ps->input->line;

		}
		| assignment TOKEN arguments
		{
			struct cmd_parse_state	*ps = &parse_state;

			$$ = xcalloc(1, sizeof *$$);
			$$->name = $2;
			$$->line = ps->input->line;

			$$->argc = $3.argc;
			$$->argv = $3.argv;
		}

condition1	: if_open commands if_close
		{
			TAILQ_INIT(&$$);
			if ($1)
				TAILQ_CONCAT(&$$, &$2, entry);
			else
				cmd_parse_free_commands(&$2);
		}
		| if_open commands if_else commands if_close
		{
			TAILQ_INIT(&$$);
			if ($1) {
				TAILQ_CONCAT(&$$, &$2, entry);
				cmd_parse_free_commands(&$4);
			} else {
				TAILQ_CONCAT(&$$, &$4, entry);
				cmd_parse_free_commands(&$2);
			}
		}
		| if_open commands elif1 if_close
		{
			TAILQ_INIT(&$$);
			if ($1) {
				TAILQ_CONCAT(&$$, &$2, entry);
				cmd_parse_free_commands(&$3.commands);
			} else if ($3.flag) {
				TAILQ_CONCAT(&$$, &$3.commands, entry);
				cmd_parse_free_commands(&$2);
			} else {
				cmd_parse_free_commands(&$2);
				cmd_parse_free_commands(&$3.commands);
			}
		}
		| if_open commands elif1 if_else commands if_close
		{
			TAILQ_INIT(&$$);
			if ($1) {
				TAILQ_CONCAT(&$$, &$2, entry);
				cmd_parse_free_commands(&$3.commands);
				cmd_parse_free_commands(&$5);
			} else if ($3.flag) {
				TAILQ_CONCAT(&$$, &$3.commands, entry);
				cmd_parse_free_commands(&$2);
				cmd_parse_free_commands(&$5);
			} else {
				TAILQ_CONCAT(&$$, &$5, entry);
				cmd_parse_free_commands(&$2);
				cmd_parse_free_commands(&$3.commands);
			}

		}

elif1		: if_elif commands
		{
			TAILQ_INIT(&$$.commands);
			if ($1)
				TAILQ_CONCAT(&$$.commands, &$2, entry);
			else
				cmd_parse_free_commands(&$2);
			$$.flag = $1;
		}
		| if_elif commands elif1
		{
			TAILQ_INIT(&$$.commands);
			if ($1) {
				$$.flag = 1;
				TAILQ_CONCAT(&$$.commands, &$2, entry);
				cmd_parse_free_commands(&$3.commands);
			} else {
				$$.flag = $3.flag;
				TAILQ_CONCAT(&$$.commands, &$3.commands, entry);
				cmd_parse_free_commands(&$2);
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

static char *
cmd_parse_get_strerror(const char *file, u_int line)
{
	return (cmd_parse_get_error(file, line, strerror(errno)));
}

static void
cmd_parse_free_command(struct cmd_parse_command *cmd)
{
	free(cmd->name);
	cmd_free_argv(cmd->argc, cmd->argv);
	free(cmd);
}

static void
cmd_parse_free_commands(struct cmd_parse_commands *cmds)
{
	struct cmd_parse_command	*cmd, *cmd1;

	TAILQ_FOREACH_SAFE(cmd, cmds, entry, cmd1) {
		TAILQ_REMOVE(cmds, cmd, entry);
		cmd_parse_free_command(cmd);
	}
}

static struct cmd_parse_commands *
cmd_parse_run_parser(FILE *f, struct cmd_parse_input *pi, char **cause)
{
	struct cmd_parse_state		*ps = &parse_state;
	struct cmd_parse_commands	*cmds;
	struct cmd_parse_scope		*scope, *scope1;
	int				 retval;

	memset(ps, 0, sizeof *ps);

	ps->f = f;
	ps->eof = 0;
	ps->input = pi;

	TAILQ_INIT(&ps->commands);
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

	cmds = xmalloc(sizeof *cmds);
	TAILQ_INIT(cmds);
	TAILQ_CONCAT(cmds, &ps->commands, entry);
	return (cmds);
}

struct cmd_parse_result *
cmd_parse_from_file(FILE *f, struct cmd_parse_input *pi)
{
	static struct cmd_parse_result	 pr;
	struct cmd_parse_input		 input;
	struct cmd_parse_commands	*cmds, *cmds2;
	struct cmd_parse_command	*cmd, *cmd2, *next, *next2, *after;
	u_int				 line = UINT_MAX;
	int				 i;
	struct cmd_list			*cmdlist = NULL, *result;
	struct cmd			*add;
	char				*alias, *cause, *s;

	if (pi == NULL) {
		memset(&input, 0, sizeof input);
		pi = &input;
	}
	memset(&pr, 0, sizeof pr);

	/*
	 * Parse the file into a list of commands.
	 */
	cmds = cmd_parse_run_parser(f, pi, &cause);
	if (cmds == NULL) {
		pr.status = CMD_PARSE_ERROR;
		pr.error = cause;
		return (&pr);
	}
	if (TAILQ_EMPTY(cmds)) {
		free(cmds);
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
		alias = cmd_get_alias(cmd->name);
		if (alias == NULL)
			continue;

		line = cmd->line;
		log_debug("%s: %u %s = %s", __func__, line, cmd->name, alias);

		f = fmemopen(alias, strlen(alias), "r");
		if (f == NULL) {
			free(alias);
			pr.status = CMD_PARSE_ERROR;
			pr.error = cmd_parse_get_strerror(pi->file, line);
			goto out;
		}
		pi->line = line;
		cmds2 = cmd_parse_run_parser(f, pi, &cause);
		fclose(f);
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
		for (i = 0; i < cmd->argc; i++)
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
	 * for each line so they get a new group (so the queue knows which ones
	 * to remove if a command fails when executed).
	 */
	result = cmd_list_new();
	TAILQ_FOREACH(cmd, cmds, entry) {
		log_debug("%s: %u %s", __func__, cmd->line, cmd->name);
		cmd_log_argv(cmd->argc, cmd->argv, __func__);

		if (cmdlist == NULL || cmd->line != line) {
			if (cmdlist != NULL) {
				cmd_list_move(result, cmdlist);
				cmd_list_free(cmdlist);
			}
			cmdlist = cmd_list_new();
		}
		line = cmd->line;

		cmd_prepend_argv(&cmd->argc, &cmd->argv, cmd->name);
		add = cmd_parse(cmd->argc, cmd->argv, pi->file, line, &cause);
		if (add == NULL) {
			cmd_list_free(result);
			pr.status = CMD_PARSE_ERROR;
			pr.error = cmd_parse_get_error(pi->file, line, cause);
			free(cause);
			goto out;
		}
		cmd_list_append(cmdlist, add);
	}
	if (cmdlist != NULL) {
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
	free(cmds);

	return (&pr);
}

struct cmd_parse_result *
cmd_parse_from_string(const char *s, struct cmd_parse_input *pi)
{
	static struct cmd_parse_result	 pr;
	struct cmd_parse_result		*prp;
	FILE				*f;

	if (*s == '\0') {
		pr.status = CMD_PARSE_EMPTY;
		pr.cmdlist = NULL;
		pr.error = NULL;
		return (&pr);
	}

	f = fmemopen((void *)s, strlen(s), "r");
	if (f == NULL) {
		pr.status = CMD_PARSE_ERROR;
		pr.cmdlist = NULL;
		pr.error = cmd_parse_get_strerror(pi->file, pi->line);
		return (NULL);
	}
	prp = cmd_parse_from_file(f, pi);
	fclose(f);
	return (prp);
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
yylex_getc(void)
{
	struct cmd_parse_state	*ps = &parse_state;
	int			 ch;

	if (ps->escapes != 0) {
		ps->escapes--;
		return ('\\');
	}
	for (;;) {
		ch = getc(ps->f);
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
			ungetc(ch, ps->f);
			ps->escapes--;
			return ('\\');
		}
		return (ch);
	}
}

static char *
yylex_get_word(int ch)
{
	struct cmd_parse_state	*ps = &parse_state;
	char			*buf;
	size_t			 len;

	len = 0;
	buf = xmalloc(1);

	do
		yylex_append1(&buf, &len, ch);
	while ((ch = yylex_getc()) != EOF && strchr(" \t\n", ch) == NULL);
	ungetc(ch, ps->f);

	buf[len] = '\0';
	log_debug("%s: %s", __func__, buf);
	return (buf);
}

static int
yylex(void)
{
	struct cmd_parse_state	*ps = &parse_state;
	char			*token, *cp;
	int			 ch, next;

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
			ps->input->line++;
			return ('\n');
		}

		if (ch == ';') {
			/*
			 * A semicolon is itself.
			 */
			return (';');
		}

		if (ch == '#') {
			/*
			 * #{ opens a format; anything else is a comment,
			 * ignore up to the end of the line.
			 */
			next = yylex_getc();
			if (next == '{') {
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
			 * % is a condition unless it is alone, then it is a
			 * token.
			 */
			yylval.token = yylex_get_word('%');
			if (strcmp(yylval.token, "%") == 0)
				return (TOKEN);
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
	int			 ch, type;
	u_int			 size, i, tmp;
	char			 s[9];
	struct utf8_data	 ud;

	switch (ch = yylex_getc()) {
	case EOF:
		return (0);
	case 'e':
		ch = '\033';
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
	if (utf8_split(tmp, &ud) != UTF8_DONE) {
		yyerror("invalid \\%c argument", type);
		return (0);
	}
	yylex_append(buf, len, ud.data, ud.size);
	return (1);
}

static int
yylex_token_variable(char **buf, size_t *len)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct environ_entry	*envent;
	int			 ch, brackets = 0;
	char			 name[BUFSIZ];
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
			ungetc(ch, ps->f);
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
				ungetc(ch, ps->f);
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
	if (envent != NULL) {
		value = envent->value;
		log_debug("%s: %s -> %s", __func__, name, value);
		yylex_append(buf, len, value, strlen(value));
	}
	return (1);
}

static int
yylex_token_tilde(char **buf, size_t *len)
{
	struct cmd_parse_state	*ps = &parse_state;
	struct environ_entry	*envent;
	int			 ch;
	char			 name[BUFSIZ];
	size_t			 namelen = 0;
	struct passwd		*pw;
	const char		*home = NULL;

	for (;;) {
		ch = yylex_getc();
		if (ch == EOF || strchr("/ \t\n\"'", ch) != NULL) {
			ungetc(ch, ps->f);
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
	struct cmd_parse_state	*ps = &parse_state;
	char			*buf;
	size_t			 len;
	enum { START,
	       NONE,
	       DOUBLE_QUOTES,
	       SINGLE_QUOTES }	 state = NONE, last = START;

	len = 0;
	buf = xmalloc(1);

	for (;;) {
		/*
		 * EOF or \n are always the end of the token. If inside quotes
		 * they are an error.
		 */
		if (ch == EOF || ch == '\n') {
			if (state != NONE)
				goto error;
			break;
		}

		/* Whitespace or ; ends a token unless inside quotes. */
		if ((ch == ' ' || ch == '\t' || ch == ';') && state == NONE)
			break;

		/*
		 * \ ~ and $ are expanded except in single quotes.
		 */
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

		/*
		 * ' and " starts or end quotes (and is consumed).
		 */
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

		/*
		 * Otherwise add the character to the buffer.
		 */
		yylex_append1(&buf, &len, ch);

	skip:
		last = state;

	next:
		ch = yylex_getc();
	}
	ungetc(ch, ps->f);

	buf[len] = '\0';
	log_debug("%s: %s", __func__, buf);
	return (buf);

error:
	free(buf);
	return (NULL);
}
