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
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/utsname.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct tmuxproc {
	const char	 *name;
	int		  exit;

	void		(*signalcb)(int);

	struct event	  ev_sighup;
	struct event	  ev_sigchld;
	struct event	  ev_sigcont;
	struct event	  ev_sigterm;
	struct event	  ev_sigusr1;
	struct event	  ev_sigusr2;
	struct event	  ev_sigwinch;
};

struct tmuxpeer {
	struct tmuxproc	*parent;

	struct imsgbuf	 ibuf;
	struct event	 event;

	int		 flags;
#define PEER_BAD 0x1

	void		(*dispatchcb)(struct imsg *, void *);
	void		 *arg;
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
		if (((n = imsg_read(&peer->ibuf)) == -1 && errno != EAGAIN) ||
		    n == 0) {
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
				if (imsg.fd != -1)
					close(imsg.fd);
				imsg_free(&imsg);
				break;
			}

			peer->dispatchcb(&imsg, peer->arg);
			imsg_free(&imsg);
		}
	}

	if (events & EV_WRITE) {
		if (msgbuf_write(&peer->ibuf.w) <= 0 && errno != EAGAIN) {
			peer->dispatchcb(NULL, peer->arg);
			return;
		}
	}

	if ((peer->flags & PEER_BAD) && peer->ibuf.w.queued == 0) {
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
	if (peer->ibuf.w.queued > 0)
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
	log_debug("on %s %s %s; libevent %s (%s)", u.sysname, u.release,
	    u.version, event_get_version(), event_get_method());

	tp = xcalloc(1, sizeof *tp);
	tp->name = xstrdup(name);

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

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

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

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

	signal_del(&tp->ev_sighup);
	signal_del(&tp->ev_sigchld);
	signal_del(&tp->ev_sigcont);
	signal_del(&tp->ev_sigterm);
	signal_del(&tp->ev_sigusr1);
	signal_del(&tp->ev_sigusr2);
	signal_del(&tp->ev_sigwinch);

	if (defaults) {
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

	peer = xcalloc(1, sizeof *peer);
	peer->parent = tp;

	peer->dispatchcb = dispatchcb;
	peer->arg = arg;

	imsg_init(&peer->ibuf, fd);
	event_set(&peer->event, fd, EV_READ, proc_event_cb, peer);

	log_debug("add peer %p: %d (%p)", peer, fd, arg);

	proc_update_event(peer);
	return (peer);
}

void
proc_remove_peer(struct tmuxpeer *peer)
{
	log_debug("remove peer %p", peer);

	event_del(&peer->event);
	imsg_clear(&peer->ibuf);

	close(peer->ibuf.fd);
	free(peer);
}

void
proc_kill_peer(struct tmuxpeer *peer)
{
	peer->flags |= PEER_BAD;
}

void
proc_toggle_log(struct tmuxproc *tp)
{
	log_toggle(tp->name);
}
