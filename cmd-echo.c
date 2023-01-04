#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

static enum cmd_retval	cmd_echo_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_echo_entry = {
	.name = "echo",
	.alias = NULL,

	.args = { "", 1, 1, NULL },
	.usage = "[message-to-echo]",

	.flags = 0,
	.exec = cmd_echo_exec
};

static enum cmd_retval cmd_echo_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args *args = cmd_get_args(self);
	const char *s = args_string(args, 0);

	if(s == NULL)
		return (CMD_RETURN_ERROR);

	cmdq_print(item, "%s", s);
	return (CMD_RETURN_NORMAL);
}
