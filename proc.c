/* $OpenBSD$ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/uio.h>
#include <sys/utsname.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(HAVE_NCURSES_H)
#include <ncurses.h>
#endif

#include "tmux.h"

struct tmuxproc {
	const char	 *name;
	int		  exit;

	void		(*signalcb)(int);

	struct event	  ev_sigint;
	struct event	  ev_sighup;
	struct event	  ev_sigchld;
	struct event	  ev_sigcont;
	struct event	  ev_sigterm;
	struct event	  ev_sigusr1;
	struct event	  ev_sigusr2;
	struct event	  ev_sigwinch;

	TAILQ_HEAD(, tmuxpeer) peers;
};

struct tmuxpeer {
	struct tmuxproc	*parent;

	struct imsgbuf	 ibuf;
	struct event	 event;
	uid_t		 uid;

	int		 flags;
#define PEER_BAD 0x1

	void		(*dispatchcb)(struct imsg *, void *);
	void		 *arg;

	TAILQ_ENTRY(tmuxpeer) entry;
};

static int	peer_check_version(struct tmuxpeer *, struct imsg *);
static void	proc_update_event(struct tmuxpeer *);

static void
proc_event_cb(__unused int fd, short events, void *arg)
{
	struct tmuxpeer	*peer = arg;
	ssize_t		 n;
	struct imsg	 imsg;

	if (!(peer->flags & PEER_BAD) && (events & EV_READ)) {
		if (imsgbuf_read(&peer->ibuf) != 1) {
			peer->dispatchcb(NULL, peer->arg);
			return;
		}
		for (;;) {
			if ((n = imsg_get(&peer->ibuf, &imsg)) == -1) {
				peer->dispatchcb(NULL, peer->arg);
				return;
			}
			if (n == 0)
				break;
			log_debug("peer %p message %d", peer, imsg.hdr.type);

			if (peer_check_version(peer, &imsg) != 0) {
				imsg_free(&imsg);
				break;
			}

			peer->dispatchcb(&imsg, peer->arg);
			imsg_free(&imsg);
		}
	}

	if (events & EV_WRITE) {
		if (imsgbuf_write(&peer->ibuf) == -1) {
			peer->dispatchcb(NULL, peer->arg);
			return;
		}
	}

	if ((peer->flags & PEER_BAD) && imsgbuf_queuelen(&peer->ibuf) == 0) {
		peer->dispatchcb(NULL, peer->arg);
		return;
	}

	proc_update_event(peer);
}

static void
proc_signal_cb(int signo, __unused short events, void *arg)
{
	struct tmuxproc	*tp = arg;

	tp->signalcb(signo);
}

static int
peer_check_version(struct tmuxpeer *peer, struct imsg *imsg)
{
	int	version;

	version = imsg->hdr.peerid & 0xff;
	if (imsg->hdr.type != MSG_VERSION && version != PROTOCOL_VERSION) {
		log_debug("peer %p bad version %d", peer, version);

		proc_send(peer, MSG_VERSION, -1, NULL, 0);
		peer->flags |= PEER_BAD;

		return (-1);
	}
	return (0);
}

static void
proc_update_event(struct tmuxpeer *peer)
{
	short	events;

	event_del(&peer->event);

	events = EV_READ;
	if (imsgbuf_queuelen(&peer->ibuf) > 0)
		events |= EV_WRITE;
	event_set(&peer->event, peer->ibuf.fd, events, proc_event_cb, peer);

	event_add(&peer->event, NULL);
}

int
proc_send(struct tmuxpeer *peer, enum msgtype type, int fd, const void *buf,
    size_t len)
{
	struct imsgbuf	*ibuf = &peer->ibuf;
	void		*vp = (void *)buf;
	int		 retval;

	if (peer->flags & PEER_BAD)
		return (-1);
	log_debug("sending message %d to peer %p (%zu bytes)", type, peer, len);

	retval = imsg_compose(ibuf, type, PROTOCOL_VERSION, -1, fd, vp, len);
	if (retval != 1)
		return (-1);
	proc_update_event(peer);
	return (0);
}

struct tmuxproc *
proc_start(const char *name)
{
	struct tmuxproc	*tp;
	struct utsname	 u;

	log_open(name);
	setproctitle("%s (%s)", name, socket_path);

	if (uname(&u) < 0)
		memset(&u, 0, sizeof u);

	log_debug("%s started (%ld): version %s, socket %s, protocol %d", name,
	    (long)getpid(), getversion(), socket_path, PROTOCOL_VERSION);
	log_debug("on %s %s %s", u.sysname, u.release, u.version);
	log_debug("using libevent %s %s", event_get_version(), event_get_method());
#ifdef HAVE_UTF8PROC
	log_debug("using utf8proc %s", utf8proc_version());
#endif
#ifdef NCURSES_VERSION
	log_debug("using ncurses %s %06u", NCURSES_VERSION, NCURSES_VERSION_PATCH);
#endif

	tp = xcalloc(1, sizeof *tp);
	tp->name = xstrdup(name);
	TAILQ_INIT(&tp->peers);

	return (tp);
}

void
proc_loop(struct tmuxproc *tp, int (*loopcb)(void))
{
	log_debug("%s loop enter", tp->name);
	do
		event_loop(EVLOOP_ONCE);
	while (!tp->exit && (loopcb == NULL || !loopcb ()));
	log_debug("%s loop exit", tp->name);
}

void
proc_exit(struct tmuxproc *tp)
{
	struct tmuxpeer	*peer;

	TAILQ_FOREACH(peer, &tp->peers, entry)
	    imsgbuf_flush(&peer->ibuf);
	tp->exit = 1;
}

void
proc_set_signals(struct tmuxproc *tp, void (*signalcb)(int))
{
	struct sigaction	sa;

	tp->signalcb = signalcb;

	memset(&sa, 0, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;

	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);
	sigaction(SIGTTIN, &sa, NULL);
	sigaction(SIGTTOU, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	signal_set(&tp->ev_sigint, SIGINT, proc_signal_cb, tp);
	signal_add(&tp->ev_sigint, NULL);
	signal_set(&tp->ev_sighup, SIGHUP, proc_signal_cb, tp);
	signal_add(&tp->ev_sighup, NULL);
	signal_set(&tp->ev_sigchld, SIGCHLD, proc_signal_cb, tp);
	signal_add(&tp->ev_sigchld, NULL);
	signal_set(&tp->ev_sigcont, SIGCONT, proc_signal_cb, tp);
	signal_add(&tp->ev_sigcont, NULL);
	signal_set(&tp->ev_sigterm, SIGTERM, proc_signal_cb, tp);
	signal_add(&tp->ev_sigterm, NULL);
	signal_set(&tp->ev_sigusr1, SIGUSR1, proc_signal_cb, tp);
	signal_add(&tp->ev_sigusr1, NULL);
	signal_set(&tp->ev_sigusr2, SIGUSR2, proc_signal_cb, tp);
	signal_add(&tp->ev_sigusr2, NULL);
	signal_set(&tp->ev_sigwinch, SIGWINCH, proc_signal_cb, tp);
	signal_add(&tp->ev_sigwinch, NULL);
}

void
proc_clear_signals(struct tmuxproc *tp, int defaults)
{
	struct sigaction	sa;

	memset(&sa, 0, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;

	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

	signal_del(&tp->ev_sigint);
	signal_del(&tp->ev_sighup);
	signal_del(&tp->ev_sigchld);
	signal_del(&tp->ev_sigcont);
	signal_del(&tp->ev_sigterm);
	signal_del(&tp->ev_sigusr1);
	signal_del(&tp->ev_sigusr2);
	signal_del(&tp->ev_sigwinch);

	if (defaults) {
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);
		sigaction(SIGCHLD, &sa, NULL);
		sigaction(SIGCONT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGUSR1, &sa, NULL);
		sigaction(SIGUSR2, &sa, NULL);
		sigaction(SIGWINCH, &sa, NULL);
	}
}

struct tmuxpeer *
proc_add_peer(struct tmuxproc *tp, int fd,
    void (*dispatchcb)(struct imsg *, void *), void *arg)
{
	struct tmuxpeer	*peer;
	gid_t		 gid;

	peer = xcalloc(1, sizeof *peer);
	peer->parent = tp;

	peer->dispatchcb = dispatchcb;
	peer->arg = arg;

	if (imsgbuf_init(&peer->ibuf, fd) == -1)
		fatal("imsgbuf_init");
	imsgbuf_allow_fdpass(&peer->ibuf);
	event_set(&peer->event, fd, EV_READ, proc_event_cb, peer);

	if (getpeereid(fd, &peer->uid, &gid) != 0)
		peer->uid = (uid_t)-1;

	log_debug("add peer %p: %d (%p)", peer, fd, arg);
	TAILQ_INSERT_TAIL(&tp->peers, peer, entry);

	proc_update_event(peer);
	return (peer);
}

void
proc_remove_peer(struct tmuxpeer *peer)
{
	TAILQ_REMOVE(&peer->parent->peers, peer, entry);
	log_debug("remove peer %p", peer);

	event_del(&peer->event);
	imsgbuf_clear(&peer->ibuf);

	close(peer->ibuf.fd);
	free(peer);
}

void
proc_kill_peer(struct tmuxpeer *peer)
{
	peer->flags |= PEER_BAD;
}

void
proc_flush_peer(struct tmuxpeer *peer)
{
	imsgbuf_flush(&peer->ibuf);
}

void
proc_toggle_log(struct tmuxproc *tp)
{
	log_toggle(tp->name);
}

pid_t
proc_fork_and_daemon(int *fd)
{
	pid_t	pid;
	int	pair[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pair) != 0)
		fatal("socketpair failed");
	switch (pid = fork()) {
	case -1:
		fatal("fork failed");
	case 0:
		close(pair[0]);
		*fd = pair[1];
		if (daemon(1, 0) != 0)
			fatal("daemon failed");
		return (0);
	default:
		close(pair[1]);
		*fd = pair[0];
		return (pid);
	}
}

uid_t
proc_get_peer_uid(struct tmuxpeer *peer)
{
	return (peer->uid);
}
