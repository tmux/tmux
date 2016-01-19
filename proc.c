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
#include <sys/uio.h>
#include <sys/utsname.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

struct tmuxproc {
	const char	 *name;
	int		  exit;

	void		(*signalcb)(int);
};

struct tmuxpeer {
	struct tmuxproc	*parent;

	struct imsgbuf	 ibuf;
	struct event	 event;

	int		 flags;
#define PEER_BAD 0x1

	void		(*dispatchcb)(struct imsg *, void *);
	void		*arg;
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

int
proc_send_s(struct tmuxpeer *peer, enum msgtype type, const char *s)
{
	return (proc_send(peer, type, -1, s, strlen(s) + 1));
}

struct tmuxproc *
proc_start(const char *name, struct event_base *base, int forkflag,
    void (*signalcb)(int))
{
	struct tmuxproc	*tp;
	struct utsname	 u;

	if (forkflag) {
		switch (fork()) {
		case -1:
			fatal("fork failed");
		case 0:
			break;
		default:
			return (NULL);
		}
		if (daemon(1, 0) != 0)
			fatal("daemon failed");

		clear_signals(0);
		if (event_reinit(base) != 0)
			fatalx("event_reinit failed");
	}

	log_open(name);

#ifdef HAVE_SETPROCTITLE
	setproctitle("%s (%s)", name, socket_path);
#endif

	if (uname(&u) < 0)
		memset(&u, 0, sizeof u);

	log_debug("%s started (%ld): socket %s, protocol %d", name,
	    (long)getpid(), socket_path, PROTOCOL_VERSION);
	log_debug("on %s %s %s; libevent %s (%s)", u.sysname, u.release,
	    u.version, event_get_version(), event_get_method());

	tp = xcalloc(1, sizeof *tp);
	tp->name = xstrdup(name);

	tp->signalcb = signalcb;
	set_signals(proc_signal_cb, tp);

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
