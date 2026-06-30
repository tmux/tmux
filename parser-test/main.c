/*
 * Standalone driver for the cmd-parse.y AST parser. Reads a config/command
 * file (argument, or stdin) and prints both forms: the debug AST dump via
 * cmd_parse_log (to stderr) and the normalized form via cmd_parse_print (to
 * stdout).
 *
 * Usage: parsetest [file]
 */

#include <locale.h>

#include "tmux.h"
#include "tmux-parser.h"

int
main(int argc, char **argv)
{
	struct cmd_parse_input	 pi;
	struct cmd_parse_tree	*tree;
	FILE			*f = stdin;
	char			*cause = NULL, *out;

	/* As tmux does at startup, so \u/\U escapes can be encoded. */
	setlocale(LC_CTYPE, "");

	memset(&pi, 0, sizeof pi);
	pi.line = 1;
	if (getenv("ONEGROUP") != NULL)
		pi.flags |= CMD_PARSE_ONEGROUP;
	if (argc > 1) {
		pi.file = argv[1];
		if ((f = fopen(argv[1], "r")) == NULL) {
			perror(argv[1]);
			return (1);
		}
	}

	tree = cmd_parse_from_file(f, &pi, &cause);
	if (f != stdin)
		fclose(f);

	if (tree == NULL) {
		fprintf(stderr, "parse error: %s\n",
		    cause != NULL ? cause : "unknown");
		free(cause);
		return (1);
	}

	fprintf(stdout, "=== AST (stderr) ===\n");
	fflush(stdout);
	cmd_parse_log(tree);

	out = cmd_parse_print(tree);
	fprintf(stdout, "=== NORMALIZED ===\n%s\n", out);
	free(out);

	cmd_parse_free(tree);
	return (0);
}
