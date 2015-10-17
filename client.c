/* $OpenBSD$ */

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
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

int		client_flags;
struct imsgbuf	client_ibuf;
struct event	client_event;
struct event	client_stdin;
enum {
	CLIENT_EXIT_NONE,
	CLIENT_EXIT_DETACHED,
	CLIENT_EXIT_DETACHED_HUP,
	CLIENT_EXIT_LOST_TTY,
	CLIENT_EXIT_TERMINATED,
	CLIENT_EXIT_LOST_SERVER,
	CLIENT_EXIT_EXITED,
	CLIENT_EXIT_SERVER_EXITED,
} client_exitreason = CLIENT_EXIT_NONE;
int		client_exitval;
enum msgtype	client_exittype;
const char     *client_exitsession;
int		client_attached;

__dead void	client_exec(const char *);
int		client_get_lock(char *);
int		client_connect(struct event_base *, char *, int);
void		client_send_identify(const char *, int);
int		client_write_one(enum msgtype, int, const void *, size_t);
int		client_write_server(enum msgtype, const void *, size_t);
void		client_update_event(void);
void		client_signal(int, short, void *);
void		client_stdin_callback(int, short, void *);
void		client_write(int, const char *, size_t);
void		client_callback(int, short, void *);
int		client_dispatch_attached(void);
int		client_dispatch_wait(void);
const char     *client_exit_message(void);

/*
 * Get server create lock. If already held then server start is happening in
 * another client, so block until the lock is released and return -1 to
 * retry. Ignore other errors - just continue and start the server without the
 * lock.
 */
int
client_get_lock(char *lockfile)
{
	int lockfd;

	if ((lockfd = open(lockfile, O_WRONLY|O_CREAT, 0600)) == -1)
		fatal("open failed");
	log_debug("lock file is %s", lockfile);

	if (flock(lockfd, LOCK_EX|LOCK_NB) == -1) {
		log_debug("flock failed: %s", strerror(errno));
		if (errno != EAGAIN)
			return (lockfd);
		while (flock(lockfd, LOCK_EX) == -1 && errno == EINTR)
			/* nothing */;
		close(lockfd);
		return (-1);
	}
	log_debug("flock succeeded");

	return (lockfd);
}

/* Connect client to server. */
int
client_connect(struct event_base *base, char *path, int start_server)
{
	struct sockaddr_un	sa;
	size_t			size;
	int			fd, lockfd = -1, locked = 0;
	char		       *lockfile = NULL;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	log_debug("socket is %s", path);

retry:
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("socket failed");

	log_debug("trying connect");
	if (connect(fd, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
		log_debug("connect failed: %s", strerror(errno));
		if (errno != ECONNREFUSED && errno != ENOENT)
			goto failed;
		if (!start_server)
			goto failed;
		close(fd);

		if (!locked) {
			xasprintf(&lockfile, "%s.lock", path);
			if ((lockfd = client_get_lock(lockfile)) == -1) {
				log_debug("didn't get lock");
				free(lockfile);
				goto retry;
			}
			log_debug("got lock");

			/*
			 * Always retry at least once, even if we got the lock,
			 * because another client could have taken the lock,
			 * started the server and released the lock between our
			 * connect() and flock().
			 */
			locked = 1;
			goto retry;
		}

		if (unlink(path) != 0 && errno != ENOENT) {
			free(lockfile);
			close(lockfd);
			return (-1);
		}
		fd = server_start(base, lockfd, lockfile);
	}

	if (locked) {
		free(lockfile);
		close(lockfd);
	}
	setblocking(fd, 0);
	return (fd);

failed:
	if (locked) {
		free(lockfile);
		close(lockfd);
	}
	close(fd);
	return (-1);
}

/* Get exit string from reason number. */
const char *
client_exit_message(void)
{
	static char msg[256];

	switch (client_exitreason) {
	case CLIENT_EXIT_NONE:
		break;
	case CLIENT_EXIT_DETACHED:
		if (client_exitsession != NULL) {
			xsnprintf(msg, sizeof msg, "detached "
			    "(from session %s)", client_exitsession);
			return (msg);
		}
		return ("detached");
	case CLIENT_EXIT_DETACHED_HUP:
		if (client_exitsession != NULL) {
			xsnprintf(msg, sizeof msg, "detached and SIGHUP "
			    "(from session %s)", client_exitsession);
			return (msg);
		}
		return ("detached and SIGHUP");
	case CLIENT_EXIT_LOST_TTY:
		return ("lost tty");
	case CLIENT_EXIT_TERMINATED:
		return ("terminated");
	case CLIENT_EXIT_LOST_SERVER:
		return ("lost server");
	case CLIENT_EXIT_EXITED:
		return ("exited");
	case CLIENT_EXIT_SERVER_EXITED:
		return ("server exited");
	}
	return ("unknown reason");
}

/* Client main loop. */
int
client_main(struct event_base *base, int argc, char **argv, int flags)
{
	struct cmd		*cmd;
	struct cmd_list		*cmdlist;
	struct msg_command_data	*data;
	int			 cmdflags, fd, i, cwd;
	const char*              ttynam;
	pid_t			 ppid;
	enum msgtype		 msg;
	char			*cause;
	struct termios		 tio, saved_tio;
	size_t			 size;

	/* Save the flags. */
	client_flags = flags;

	/* Set up the initial command. */
	cmdflags = 0;
	if (shell_cmd != NULL) {
		msg = MSG_SHELL;
		cmdflags = CMD_STARTSERVER;
	} else if (argc == 0) {
		msg = MSG_COMMAND;
		cmdflags = CMD_STARTSERVER;
	} else {
		msg = MSG_COMMAND;

		/*
		 * It sucks parsing the command string twice (in client and
		 * later in server) but it is necessary to get the start server
		 * flag.
		 */
		cmdlist = cmd_list_parse(argc, argv, NULL, 0, &cause);
		if (cmdlist == NULL) {
			fprintf(stderr, "%s\n", cause);
			return (1);
		}
		cmdflags &= ~CMD_STARTSERVER;
		TAILQ_FOREACH(cmd, &cmdlist->list, qentry) {
			if (cmd->entry->flags & CMD_STARTSERVER)
				cmdflags |= CMD_STARTSERVER;
		}
		cmd_list_free(cmdlist);
	}

	/* Establish signal handlers. */
	set_signals(client_signal);

	/* Initialize the client socket and start the server. */
	fd = client_connect(base, socket_path, cmdflags & CMD_STARTSERVER);
	if (fd == -1) {
		if (errno == ECONNREFUSED) {
			fprintf(stderr, "no server running on %s\n",
			    socket_path);
		} else {
			fprintf(stderr, "error connecting to %s (%s)\n",
			    socket_path, strerror(errno));
		}
		return (1);
	}

	/* Save these before pledge(). */
	if ((cwd = open(".", O_RDONLY)) == -1)
		cwd = open("/", O_RDONLY);
	if ((ttynam = ttyname(STDIN_FILENO)) == NULL)
		ttynam = "";

#ifdef __OpenBSD__
	/*
	 * Drop privileges for client. "proc exec" is needed for -c and for
	 * locking (which uses system(3)).
	 *
	 * "tty" is needed to restore termios(4) and also for some reason -CC
	 * does not work properly without it (input is not recognised).
	 *
	 * "sendfd" is dropped later in client_dispatch_wait().
	 */
	if (pledge("stdio unix sendfd proc exec tty", NULL) != 0)
		fatal("pledge failed");
#endif

	/* Free stuff that is not used in the client. */
	options_free(&global_options);
	options_free(&global_s_options);
	options_free(&global_w_options);
	environ_free(&global_environ);

	/* Set process title, log and signals now this is the client. */
#ifdef HAVE_SETPROCTITLE
	setproctitle("client (%s)", socket_path);
#endif
	logfile("client");

	/* Create imsg. */
	imsg_init(&client_ibuf, fd);
	event_set(&client_event, fd, EV_READ, client_callback, NULL);

	/* Create stdin handler. */
	setblocking(STDIN_FILENO, 0);
	event_set(&client_stdin, STDIN_FILENO, EV_READ|EV_PERSIST,
	    client_stdin_callback, NULL);
	if (client_flags & CLIENT_CONTROLCONTROL) {
		if (tcgetattr(STDIN_FILENO, &saved_tio) != 0) {
			fprintf(stderr, "tcgetattr failed: %s\n",
			    strerror(errno));
			return (1);
		}
		cfmakeraw(&tio);
		tio.c_iflag = ICRNL|IXANY;
		tio.c_oflag = OPOST|ONLCR;
#ifdef NOKERNINFO
		tio.c_lflag = NOKERNINFO;
#endif
		tio.c_cflag = CREAD|CS8|HUPCL;
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
		cfsetispeed(&tio, cfgetispeed(&saved_tio));
		cfsetospeed(&tio, cfgetospeed(&saved_tio));
		tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	}

	/* Send identify messages. */
	client_send_identify(ttynam, cwd); /* closes cwd */

	/* Send first command. */
	if (msg == MSG_COMMAND) {
		/* How big is the command? */
		size = 0;
		for (i = 0; i < argc; i++)
			size += strlen(argv[i]) + 1;
		data = xmalloc((sizeof *data) + size);

		/* Prepare command for server. */
		data->argc = argc;
		if (cmd_pack_argv(argc, argv, (char *)(data + 1), size) != 0) {
			fprintf(stderr, "command too long\n");
			free(data);
			return (1);
		}
		size += sizeof *data;

		/* Send the command. */
		if (client_write_server(msg, data, size) != 0) {
			fprintf(stderr, "failed to send command\n");
			free(data);
			return (1);
		}
		free(data);
	} else if (msg == MSG_SHELL)
		client_write_server(msg, NULL, 0);

	/* Set the event and dispatch. */
	client_update_event();
	event_dispatch();

	/* Print the exit message, if any, and exit. */
	if (client_attached) {
		if (client_exitreason != CLIENT_EXIT_NONE)
			printf("[%s]\n", client_exit_message());

		ppid = getppid();
		if (client_exittype == MSG_DETACHKILL && ppid > 1)
			kill(ppid, SIGHUP);
	} else if (client_flags & CLIENT_CONTROLCONTROL) {
		if (client_exitreason != CLIENT_EXIT_NONE)
			printf("%%exit %s\n", client_exit_message());
		else
			printf("%%exit\n");
		printf("\033\\");
		tcsetattr(STDOUT_FILENO, TCSAFLUSH, &saved_tio);
	}
	setblocking(STDIN_FILENO, 1);
	return (client_exitval);
}

/* Send identify messages to server. */
void
client_send_identify(const char *ttynam, int cwd)
{
	const char	 *s;
	char		**ss;
	size_t		  sslen;
	int		  fd, flags = client_flags;
	pid_t		  pid;

	client_write_one(MSG_IDENTIFY_FLAGS, -1, &flags, sizeof flags);

	if ((s = getenv("TERM")) == NULL)
		s = "";
	client_write_one(MSG_IDENTIFY_TERM, -1, s, strlen(s) + 1);

	client_write_one(MSG_IDENTIFY_TTYNAME, -1, ttynam, strlen(ttynam) + 1);
	client_write_one(MSG_IDENTIFY_CWD, cwd, NULL, 0);

	if ((fd = dup(STDIN_FILENO)) == -1)
		fatal("dup failed");
	client_write_one(MSG_IDENTIFY_STDIN, fd, NULL, 0);

	pid = getpid();
	client_write_one(MSG_IDENTIFY_CLIENTPID, -1, &pid, sizeof pid);

	for (ss = environ; *ss != NULL; ss++) {
		sslen = strlen(*ss) + 1;
		if (sslen <= MAX_IMSGSIZE - IMSG_HEADER_SIZE)
			client_write_one(MSG_IDENTIFY_ENVIRON, -1, *ss, sslen);
	}

	client_write_one(MSG_IDENTIFY_DONE, -1, NULL, 0);
}

/* Helper to send one message. */
int
client_write_one(enum msgtype type, int fd, const void *buf, size_t len)
{
	int	retval;

	retval = imsg_compose(&client_ibuf, type, PROTOCOL_VERSION, -1, fd,
	    (void *)buf, len);
	if (retval != 1)
		return (-1);
	return (0);
}

/* Write a message to the server without a file descriptor. */
int
client_write_server(enum msgtype type, const void *buf, size_t len)
{
	int	retval;

	retval = client_write_one(type, -1, buf, len);
	if (retval == 0)
		client_update_event();
	return (retval);
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
	event_set(&client_event, client_ibuf.fd, events, client_callback, NULL);
	event_add(&client_event, NULL);
}

/* Callback to handle signals in the client. */
void
client_signal(int sig, unused short events, unused void *arg)
{
	struct sigaction sigact;
	int		 status;

	if (sig == SIGCHLD)
		waitpid(WAIT_ANY, &status, WNOHANG);
	else if (!client_attached) {
		if (sig == SIGTERM)
			event_loopexit(NULL);
	} else {
		switch (sig) {
		case SIGHUP:
			client_exitreason = CLIENT_EXIT_LOST_TTY;
			client_exitval = 1;
			client_write_server(MSG_EXITING, NULL, 0);
			break;
		case SIGTERM:
			client_exitreason = CLIENT_EXIT_TERMINATED;
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
void
client_callback(unused int fd, short events, unused void *arg)
{
	ssize_t	n;
	int	retval;

	if (events & EV_READ) {
		if ((n = imsg_read(&client_ibuf)) == -1 || n == 0)
			goto lost_server;
		if (client_attached)
			retval = client_dispatch_attached();
		else
			retval = client_dispatch_wait();
		if (retval != 0) {
			event_loopexit(NULL);
			return;
		}
	}

	if (events & EV_WRITE) {
		if (msgbuf_write(&client_ibuf.w) <= 0 && errno != EAGAIN)
			goto lost_server;
	}

	client_update_event();
	return;

lost_server:
	client_exitreason = CLIENT_EXIT_LOST_SERVER;
	client_exitval = 1;
	event_loopexit(NULL);
}

/* Callback for client stdin read events. */
void
client_stdin_callback(unused int fd, unused short events, unused void *arg)
{
	struct msg_stdin_data	data;

	data.size = read(STDIN_FILENO, data.data, sizeof data.data);
	if (data.size < 0 && (errno == EINTR || errno == EAGAIN))
		return;

	client_write_server(MSG_STDIN, &data, sizeof data);
	if (data.size <= 0)
		event_del(&client_stdin);
	client_update_event();
}

/* Force write to file descriptor. */
void
client_write(int fd, const char *data, size_t size)
{
	ssize_t	used;

	while (size != 0) {
		used = write(fd, data, size);
		if (used == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			break;
		}
		data += used;
		size -= used;
	}
}

/* Run command in shell; used for -c. */
__dead void
client_exec(const char *shell)
{
	const char	*name, *ptr;
	char		*argv0;

	log_debug("shell %s, command %s", shell, shell_cmd);

	ptr = strrchr(shell, '/');
	if (ptr != NULL && *(ptr + 1) != '\0')
		name = ptr + 1;
	else
		name = shell;
	if (client_flags & CLIENT_LOGIN)
		xasprintf(&argv0, "-%s", name);
	else
		xasprintf(&argv0, "%s", name);
	setenv("SHELL", shell, 1);

	setblocking(STDIN_FILENO, 1);
	setblocking(STDOUT_FILENO, 1);
	setblocking(STDERR_FILENO, 1);
	closefrom(STDERR_FILENO + 1);

	execl(shell, argv0, "-c", shell_cmd, (char *) NULL);
	fatal("execl failed");
}

/* Dispatch imsgs when in wait state (before MSG_READY). */
int
client_dispatch_wait(void)
{
	struct imsg		 imsg;
	char			*data;
	ssize_t			 n, datalen;
	struct msg_stdout_data	 stdoutdata;
	struct msg_stderr_data	 stderrdata;
	int			 retval;
#ifdef __OpenBSD__
	static int		 pledge_applied;

	/*
	 * "sendfd" is no longer required once all of the identify messages
	 * have been sent. We know the server won't send us anything until that
	 * point (because we don't ask it to), so we can drop "sendfd" once we
	 * get the first message from the server.
	 */
	if (!pledge_applied) {
		if (pledge("stdio unix proc exec tty", NULL) != 0)
			fatal("pledge failed");
		pledge_applied = 1;
	};
#endif

	for (;;) {
		if ((n = imsg_get(&client_ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return (0);

		data = imsg.data;
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		log_debug("got %u from server", imsg.hdr.type);
		switch (imsg.hdr.type) {
		case MSG_EXIT:
		case MSG_SHUTDOWN:
			if (datalen != sizeof retval && datalen != 0)
				fatalx("bad MSG_EXIT size");
			if (datalen == sizeof retval) {
				memcpy(&retval, data, sizeof retval);
				client_exitval = retval;
			}
			imsg_free(&imsg);
			return (-1);
		case MSG_READY:
			if (datalen != 0)
				fatalx("bad MSG_READY size");

			event_del(&client_stdin);
			client_attached = 1;
			client_write_server(MSG_RESIZE, NULL, 0);
			break;
		case MSG_STDIN:
			if (datalen != 0)
				fatalx("bad MSG_STDIN size");

			event_add(&client_stdin, NULL);
			break;
		case MSG_STDOUT:
			if (datalen != sizeof stdoutdata)
				fatalx("bad MSG_STDOUT size");
			memcpy(&stdoutdata, data, sizeof stdoutdata);

			client_write(STDOUT_FILENO, stdoutdata.data,
			    stdoutdata.size);
			break;
		case MSG_STDERR:
			if (datalen != sizeof stderrdata)
				fatalx("bad MSG_STDERR size");
			memcpy(&stderrdata, data, sizeof stderrdata);

			client_write(STDERR_FILENO, stderrdata.data,
			    stderrdata.size);
			break;
		case MSG_VERSION:
			if (datalen != 0)
				fatalx("bad MSG_VERSION size");

			fprintf(stderr, "protocol version mismatch "
			    "(client %d, server %u)\n", PROTOCOL_VERSION,
			    imsg.hdr.peerid);
			client_exitval = 1;

			imsg_free(&imsg);
			return (-1);
		case MSG_SHELL:
			if (datalen == 0 || data[datalen - 1] != '\0')
				fatalx("bad MSG_SHELL string");

			clear_signals(0);
			client_exec(data);
			/* NOTREACHED */
		case MSG_DETACH:
		case MSG_DETACHKILL:
			client_write_server(MSG_EXITING, NULL, 0);
			break;
		case MSG_EXITED:
			imsg_free(&imsg);
			return (-1);
		}

		imsg_free(&imsg);
	}
}

/* Dispatch imsgs in attached state (after MSG_READY). */
int
client_dispatch_attached(void)
{
	struct imsg		 imsg;
	struct sigaction	 sigact;
	char			*data;
	ssize_t			 n, datalen;

	for (;;) {
		if ((n = imsg_get(&client_ibuf, &imsg)) == -1)
			fatalx("imsg_get failed");
		if (n == 0)
			return (0);

		data = imsg.data;
		datalen = imsg.hdr.len - IMSG_HEADER_SIZE;

		log_debug("got %u from server", imsg.hdr.type);
		switch (imsg.hdr.type) {
		case MSG_DETACH:
		case MSG_DETACHKILL:
			if (datalen == 0 || data[datalen - 1] != '\0')
				fatalx("bad MSG_DETACH string");

			client_exitsession = xstrdup(data);
			client_exittype = imsg.hdr.type;
			if (imsg.hdr.type == MSG_DETACHKILL)
				client_exitreason = CLIENT_EXIT_DETACHED_HUP;
			else
				client_exitreason = CLIENT_EXIT_DETACHED;
			client_write_server(MSG_EXITING, NULL, 0);
			break;
		case MSG_EXIT:
			if (datalen != 0 && datalen != sizeof (int))
				fatalx("bad MSG_EXIT size");

			client_write_server(MSG_EXITING, NULL, 0);
			client_exitreason = CLIENT_EXIT_EXITED;
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
			client_exitreason = CLIENT_EXIT_SERVER_EXITED;
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
			if (datalen == 0 || data[datalen - 1] != '\0')
				fatalx("bad MSG_LOCK string");

			system(data);
			client_write_server(MSG_UNLOCK, NULL, 0);
			break;
		}

		imsg_free(&imsg);
	}
}
