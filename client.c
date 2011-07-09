/* $Id$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct imsgbuf	client_ibuf;
struct event	client_event;
const char     *client_exitmsg;
int		client_exitval;
enum msgtype	client_exittype;
int		client_attached;

int		client_connect(char *, int);
void		client_send_identify(int);
void		client_send_environ(void);
void		client_write_server(enum msgtype, void *, size_t);
void		client_update_event(void);
void		client_signal(int, short, void *);
void		client_callback(int, short, void *);
int		client_dispatch_attached(void);
int		client_dispatch_wait(void *);

/* Connect client to server. */
int
client_connect(char *path, int start_server)
{
	struct sockaddr_un	sa;
	size_t			size;
	int			fd;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		return (-1);
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("socket failed");

	if (connect(fd, (struct sockaddr *) &sa, SUN_LEN(&sa)) == -1) {
		if (!start_server)
			goto failed;
		switch (errno) {
		case ECONNREFUSED:
			if (unlink(path) != 0)
				goto failed;
			/* FALLTHROUGH */
		case ENOENT:
			if ((fd = server_start()) == -1)
				goto failed;
			break;
		default:
			goto failed;
		}
	}

	setblocking(fd, 0);
	return (fd);

failed:
	close(fd);
	return (-1);
}

/* Client main loop. */
int
client_main(int argc, char **argv, int flags)
{
	struct cmd		*cmd;
	struct cmd_list		*cmdlist;
	struct msg_command_data	 cmddata;
	int			 cmdflags, fd;
	pid_t			 ppid;
	enum msgtype		 msg;
	char			*cause;

	/* Set up the initial command. */
	cmdflags = 0;
	if (shell_cmd != NULL) {
		msg = MSG_SHELL;
		cmdflags = CMD_STARTSERVER;
	} else if (argc == 0) {
		msg = MSG_COMMAND;
		cmdflags = CMD_STARTSERVER|CMD_SENDENVIRON|CMD_CANTNEST;
	} else {
		msg = MSG_COMMAND;

		/*
		 * It sucks parsing the command string twice (in client and
		 * later in server) but it is necessary to get the start server
		 * flag.
		 */
		if ((cmdlist = cmd_list_parse(argc, argv, &cause)) == NULL) {
			log_warnx("%s", cause);
			return (1);
		}
		cmdflags &= ~CMD_STARTSERVER;
		TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
			if (cmd->entry->flags & CMD_STARTSERVER)
				cmdflags |= CMD_STARTSERVER;
			if (cmd->entry->flags & CMD_SENDENVIRON)
				cmdflags |= CMD_SENDENVIRON;
			if (cmd->entry->flags & CMD_CANTNEST)
				cmdflags |= CMD_CANTNEST;
		}
		cmd_list_free(cmdlist);
	}

	/*
	 * Check if this could be a nested session, if the command can't nest:
	 * if the socket path matches $TMUX, this is probably the same server.
	 */
	if (shell_cmd == NULL && environ_path != NULL &&
	    cmdflags & CMD_CANTNEST && strcmp(socket_path, environ_path) == 0) {
		log_warnx("sessions should be nested with care. "
		    "unset $TMUX to force.");
		return (1);
	}

	/* Initialise the client socket and start the server. */
	fd = client_connect(socket_path, cmdflags & CMD_STARTSERVER);
	if (fd == -1) {
		log_warn("failed to connect to server");
		return (1);
	}

	/* Set process title, log and signals now this is the client. */
#ifdef HAVE_SETPROCTITLE
	setproctitle("client (%s)", socket_path);
#endif
	logfile("client");

	/* Create imsg. */
	imsg_init(&client_ibuf, fd);
	event_set(&client_event, fd, EV_READ, client_callback, shell_cmd);

	/* Establish signal handlers. */
	set_signals(client_signal);

	/* Send initial environment. */
	if (cmdflags & CMD_SENDENVIRON)
		client_send_environ();
	client_send_identify(flags);

	/* Send first command. */
	if (msg == MSG_COMMAND) {
		/* Fill in command line arguments. */
		cmddata.pid = environ_pid;
		cmddata.idx = environ_idx;

		/* Prepare command for server. */
		cmddata.argc = argc;
		if (cmd_pack_argv(
		    argc, argv, cmddata.argv, sizeof cmddata.argv) != 0) {
			log_warnx("command too long");
			return (1);
		}

		client_write_server(msg, &cmddata, sizeof cmddata);
	} else if (msg == MSG_SHELL)
		client_write_server(msg, NULL, 0);

	/* Set the event and dispatch. */
	client_update_event();
	event_dispatch();

	/* Print the exit message, if any, and exit. */
	if (client_attached) {
		if (client_exitmsg != NULL && !login_shell)
			printf("[%s]\n", client_exitmsg);

		ppid = getppid();
		if (client_exittype == MSG_DETACHKILL && ppid > 1)
			kill(ppid, SIGHUP);
	}
	return (client_exitval);
}

/* Send identify message to server with the file descriptors. */
void
client_send_identify(int flags)
{
	struct msg_identify_data	data;
	char			       *term;
	int				fd;

	data.flags = flags;

	if (getcwd(data.cwd, sizeof data.cwd) == NULL)
		*data.cwd = '\0';

	term = getenv("TERM");
	if (term == NULL ||
	    strlcpy(data.term, term, sizeof data.term) >= sizeof data.term)
		*data.term = '\0';

	if ((fd = dup(STDIN_FILENO)) == -1)
		fatal("dup failed");
	imsg_compose(&client_ibuf,
	    MSG_IDENTIFY, PROTOCOL_VERSION, -1, fd, &data, sizeof data);

	if ((fd = dup(STDOUT_FILENO)) == -1)
		fatal("dup failed");
	imsg_compose(&client_ibuf,
	    MSG_STDOUT, PROTOCOL_VERSION, -1, fd, NULL, 0);

	if ((fd = dup(STDERR_FILENO)) == -1)
		fatal("dup failed");
	imsg_compose(&client_ibuf,
	    MSG_STDERR, PROTOCOL_VERSION, -1, fd, NULL, 0);
}

/* Forward entire environment to server. */
void
client_send_environ(void)
{
	struct msg_environ_data	data;
	char		      **var;

	for (var = environ; *var != NULL; var++) {
		if (strlcpy(data.var, *var, sizeof data.var) >= sizeof data.var)
			continue;
		client_write_server(MSG_ENVIRON, &data, sizeof data);
	}
}

/* Write a message to the server without a file descriptor. */
void
client_write_server(enum msgtype type, void *buf, size_t len)
{
	imsg_compose(&client_ibuf, type, PROTOCOL_VERSION, -1, -1, buf, len);
}

/* Update client event based on whether it needs to read or read and write. */
void
client_update_event(void)
{
	short	events;

	event_del(&client_event);
	events = EV_READ;
	if (client_ibuf.w.queued > 0)
		events |= EV_WRITE;
	event_set(
	    &client_event, client_ibuf.fd, events, client_callback, shell_cmd);
	event_add(&client_event, NULL);
}

/* Callback to handle signals in the client. */
/* ARGSUSED */
void
client_signal(int sig, unused short events, unused void *data)
{
	struct sigaction sigact;
	int		 status;

	if (!client_attached) {
		switch (sig) {
		case SIGCHLD:
			waitpid(WAIT_ANY, &status, WNOHANG);
			break;
		case SIGTERM:
			event_loopexit(NULL);
			break;
		}
	} else {
		switch (sig) {
		case SIGHUP:
			client_exitmsg = "lost tty";
			client_exitval = 1;
			client_write_server(MSG_EXITING, NULL, 0);
			break;
		case SIGTERM:
			client_exitmsg = "terminated";
			client_exitval = 1;
			client_write_server(MSG_EXITING, NULL, 0);
			break;
		case SIGWINCH:
			client_write_server(MSG_RESIZE, NULL, 0);
			break;
		case SIGCONT:
			memset(&sigact, 0, sizeof sigact);
			sigemptyset(&sigact.sa_mask);
			sigact.sa_flags = SA_RESTART;
			sigact.sa_handler = SIG_IGN;
			if (sigaction(SIGTSTP, &sigact, NULL) != 0)
				fatal("sigaction failed");
			client_write_server(MSG_WAKEUP, NULL, 0);
			break;
		}
	}

	client_update_event();
}

/* Callback for client imsg read events. */
/* ARGSUSED */
void
client_callback(unused int fd, short events, void *data)
{
	ssize_t	n;
	int	retval;

	if (events & EV_READ) {
		if ((n = imsg_read(&client_ibuf)) == -1 || n == 0)
			goto lost_server;
		if (client_attached)
			retval = client_dispatch_attached();
		else
			retval = client_dispatch_wait(data);
		if (retval != 0) {
			event_loopexit(NULL);
			return;
		}
	}

	if (events & EV_WRITE) {
		if (msgbuf_write(&client_ibuf.w) < 0)
			goto lost_server;
	}

	client_update_event();
	return;

lost_server:
	client_exitmsg = "lost server";
	client_exitval = 1;
	event_loopexit(NULL);
}

/* Dispatch imsgs when in wait state (before MSG_READY). */
int
client_dispatch_wait(void *data)
{
	struct imsg		imsg;
	ssize_t			n, datalen;
	struct msg_shell_data	shelldata;
	struct msg_exit_data	exitdata;
	const char             *shellcmd = data;

	if ((n = imsg_read(&client_ibuf)) == -1 || n == 0)
		fatalx("imsg_read failed");

	for (;;) {
		if ((n = imsg_get(&client_ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		switch (imsg.hdr.type) {
		case MSG_EXIT:
		case MSG_SHUTDOWN:
			if (datalen != sizeof exitdata) {
				if (datalen != 0)
					fatalx("bad MSG_EXIT size");
			} else {
				memcpy(&exitdata, imsg.data, sizeof exitdata);
				client_exitval = exitdata.retcode;
			}
			imsg_free(&imsg);
			return (-1);
		case MSG_READY:
			if (datalen != 0)
				fatalx("bad MSG_READY size");

			client_attached = 1;
			break;
		case MSG_VERSION:
			if (datalen != 0)
				fatalx("bad MSG_VERSION size");

			log_warnx("protocol version mismatch (client %u, "
			    "server %u)", PROTOCOL_VERSION, imsg.hdr.peerid);
			client_exitval = 1;

			imsg_free(&imsg);
			return (-1);
		case MSG_SHELL:
			if (datalen != sizeof shelldata)
				fatalx("bad MSG_SHELL size");
			memcpy(&shelldata, imsg.data, sizeof shelldata);
			shelldata.shell[(sizeof shelldata.shell) - 1] = '\0';

			clear_signals(0);

			shell_exec(shelldata.shell, shellcmd);
			/* NOTREACHED */
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}

/* Dispatch imsgs in attached state (after MSG_READY). */
/* ARGSUSED */
int
client_dispatch_attached(void)
{
	struct imsg		imsg;
	struct msg_lock_data	lockdata;
	struct sigaction	sigact;
	ssize_t			n, datalen;

	for (;;) {
		if ((n = imsg_get(&client_ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return (0);
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		log_debug("client got %d", imsg.hdr.type);
		switch (imsg.hdr.type) {
		case MSG_DETACHKILL:
		case MSG_DETACH:
			if (datalen != 0)
				fatalx("bad MSG_DETACH size");

			client_exittype = imsg.hdr.type;
			if (imsg.hdr.type == MSG_DETACHKILL)
				client_exitmsg = "detached and SIGHUP";
			else
				client_exitmsg = "detached";
			client_write_server(MSG_EXITING, NULL, 0);
			break;
		case MSG_EXIT:
			if (datalen != 0 &&
			    datalen != sizeof (struct msg_exit_data))
				fatalx("bad MSG_EXIT size");

			client_write_server(MSG_EXITING, NULL, 0);
			client_exitmsg = "exited";
			break;
		case MSG_EXITED:
			if (datalen != 0)
				fatalx("bad MSG_EXITED size");

			imsg_free(&imsg);
			return (-1);
		case MSG_SHUTDOWN:
			if (datalen != 0)
				fatalx("bad MSG_SHUTDOWN size");

			client_write_server(MSG_EXITING, NULL, 0);
			client_exitmsg = "server exited";
			client_exitval = 1;
			break;
		case MSG_SUSPEND:
			if (datalen != 0)
				fatalx("bad MSG_SUSPEND size");

			memset(&sigact, 0, sizeof sigact);
			sigemptyset(&sigact.sa_mask);
			sigact.sa_flags = SA_RESTART;
			sigact.sa_handler = SIG_DFL;
			if (sigaction(SIGTSTP, &sigact, NULL) != 0)
				fatal("sigaction failed");
			kill(getpid(), SIGTSTP);
			break;
		case MSG_LOCK:
			if (datalen != sizeof lockdata)
				fatalx("bad MSG_LOCK size");
			memcpy(&lockdata, imsg.data, sizeof lockdata);

			lockdata.cmd[(sizeof lockdata.cmd) - 1] = '\0';
			system(lockdata.cmd);
			client_write_server(MSG_UNLOCK, NULL, 0);
			break;
		default:
			fatalx("unexpected message");
		}

		imsg_free(&imsg);
	}
}
