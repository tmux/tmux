/* $OpenBSD$ */

/*
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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Freeze or thaw a pane's process using the Linux cgroup v2 freezer
 * (cgroup.freeze), instead of a job-control signal.
 *
 * A signal-based suspend (SIGSTOP/SIGTSTP) is a wait()-visible state change:
 * both an interactive shell's own job control and tmux's own SIGCHLD
 * handler (server_child_stopped(), see server.c) react to it - the shell by
 * reclaiming the terminal, and tmux by immediately sending the process
 * SIGCONT again. This makes it impossible to leave a pane's process
 * genuinely suspended while it is (or is the ancestor of) tmux's own child.
 *
 * The cgroup v2 freezer stops the kernel from scheduling every task in a
 * cgroup without delivering any signal and without producing any
 * wait()-visible transition, so neither of those reflexes ever fires.
 */

#define CMD_FREEZE_PROCESS_CGROUP_DIR "tmux-freeze"
#define CMD_FREEZE_PROCESS_CGROUP_MOUNT "/sys/fs/cgroup"

static enum cmd_retval	cmd_freeze_process_cgroup_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_freeze_process_cgroup_entry = {
	.name = "freeze-process-cgroup",
	.alias = NULL,

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_freeze_process_cgroup_exec
};

const struct cmd_entry cmd_thaw_process_cgroup_entry = {
	.name = "thaw-process-cgroup",
	.alias = NULL,

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_freeze_process_cgroup_exec
};

/*
 * Work out the cgroup v2 directory to freeze this pid in, creating it as a
 * child of the pid's current cgroup if necessary. If the pid is already
 * inside a directory this function created earlier, that is reused rather
 * than nesting another one inside it. Returns an allocated path, or NULL
 * with *cause set on error.
 */
static char *
cmd_freeze_process_cgroup_path(pid_t pid, char **cause)
{
	FILE	*f;
	char	 procpath[64], line[PATH_MAX + 8], *nl, *base, *dir;

	xsnprintf(procpath, sizeof procpath, "/proc/%ld/cgroup", (long)pid);
	if ((f = fopen(procpath, "r")) == NULL) {
		xasprintf(cause, "can't open %s: %s", procpath,
		    strerror(errno));
		return (NULL);
	}
	if (fgets(line, sizeof line, f) == NULL ||
	    strncmp(line, "0::", 3) != 0) {
		fclose(f);
		xasprintf(cause, "%s is not on a cgroup v2 (unified) "
		    "hierarchy", procpath);
		return (NULL);
	}
	fclose(f);
	if ((nl = strchr(line, '\n')) != NULL)
		*nl = '\0';

	base = strrchr(line + 3, '/');
	if (base != NULL && strncmp(base + 1,
	    CMD_FREEZE_PROCESS_CGROUP_DIR,
	    strlen(CMD_FREEZE_PROCESS_CGROUP_DIR)) == 0) {
		xasprintf(&dir, "%s%s", CMD_FREEZE_PROCESS_CGROUP_MOUNT,
		    line + 3);
		return (dir);
	}

	xasprintf(&dir, "%s%s/%s-%ld", CMD_FREEZE_PROCESS_CGROUP_MOUNT,
	    line + 3, CMD_FREEZE_PROCESS_CGROUP_DIR, (long)pid);
	if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
		xasprintf(cause, "can't create %s: %s", dir, strerror(errno));
		free(dir);
		return (NULL);
	}
	return (dir);
}

static int
cmd_freeze_process_cgroup_write(const char *dir, const char *file,
    const char *value, char **cause)
{
	char	*path;
	int	 fd;
	ssize_t	 n;

	xasprintf(&path, "%s/%s", dir, file);
	if ((fd = open(path, O_WRONLY)) == -1) {
		xasprintf(cause, "can't open %s: %s", path, strerror(errno));
		free(path);
		return (-1);
	}
	n = write(fd, value, strlen(value));
	close(fd);
	if (n == -1 || (size_t)n != strlen(value)) {
		xasprintf(cause, "can't write to %s: %s", path,
		    strerror(errno));
		free(path);
		return (-1);
	}
	free(path);
	return (0);
}

static enum cmd_retval
cmd_freeze_process_cgroup_exec(struct cmd *self, struct cmdq_item *item)
{
	struct window_pane	*wp = cmdq_get_target(item)->wp;
	int			 freeze;
	char			*dir, *cause = NULL, pidstr[32];
	pid_t			 pid = wp->pid;

	if (pid <= 0) {
		cmdq_error(item, "pane has no process");
		return (CMD_RETURN_ERROR);
	}
	freeze = (cmd_get_entry(self) == &cmd_freeze_process_cgroup_entry);

	if ((dir = cmd_freeze_process_cgroup_path(pid, &cause)) == NULL) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	if (freeze) {
		xsnprintf(pidstr, sizeof pidstr, "%ld", (long)pid);
		if (cmd_freeze_process_cgroup_write(dir, "cgroup.procs",
		    pidstr, &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			free(dir);
			return (CMD_RETURN_ERROR);
		}
	}

	if (cmd_freeze_process_cgroup_write(dir, "cgroup.freeze",
	    freeze ? "1" : "0", &cause) != 0) {
		cmdq_error(item, "%s", cause);
		free(cause);
		free(dir);
		return (CMD_RETURN_ERROR);
	}

	free(dir);
	return (CMD_RETURN_NORMAL);
}
