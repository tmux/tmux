/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2010 Romain Francoise <rfrancoise@debian.org>
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

#include <string.h>
#include <signal.h>

#include "tmux.h"

struct event	ev_sighup;
struct event	ev_sigchld;
struct event	ev_sigcont;
struct event	ev_sigterm;
struct event	ev_sigusr1;
struct event	ev_sigwinch;

void
set_signals(void (*handler)(int, short, void *), void *arg)
{
	struct sigaction	sigact;

	memset(&sigact, 0, sizeof sigact);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_IGN;
	if (sigaction(SIGINT, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGPIPE, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &sigact, NULL) != 0)
		fatal("sigaction failed");

	signal_set(&ev_sighup, SIGHUP, handler, arg);
	signal_add(&ev_sighup, NULL);
	signal_set(&ev_sigchld, SIGCHLD, handler, arg);
	signal_add(&ev_sigchld, NULL);
	signal_set(&ev_sigcont, SIGCONT, handler, arg);
	signal_add(&ev_sigcont, NULL);
	signal_set(&ev_sigterm, SIGTERM, handler, arg);
	signal_add(&ev_sigterm, NULL);
	signal_set(&ev_sigusr1, SIGUSR1, handler, arg);
	signal_add(&ev_sigusr1, NULL);
	signal_set(&ev_sigwinch, SIGWINCH, handler, arg);
	signal_add(&ev_sigwinch, NULL);
}

void
clear_signals(int after_fork)
{
	struct sigaction	sigact;

	memset(&sigact, 0, sizeof sigact);
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGPIPE, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGUSR2, &sigact, NULL) != 0)
		fatal("sigaction failed");
	if (sigaction(SIGTSTP, &sigact, NULL) != 0)
		fatal("sigaction failed");

	if (after_fork) {
		if (sigaction(SIGHUP, &sigact, NULL) != 0)
			fatal("sigaction failed");
		if (sigaction(SIGCHLD, &sigact, NULL) != 0)
			fatal("sigaction failed");
		if (sigaction(SIGCONT, &sigact, NULL) != 0)
			fatal("sigaction failed");
		if (sigaction(SIGTERM, &sigact, NULL) != 0)
			fatal("sigaction failed");
		if (sigaction(SIGUSR1, &sigact, NULL) != 0)
			fatal("sigaction failed");
		if (sigaction(SIGWINCH, &sigact, NULL) != 0)
			fatal("sigaction failed");
	} else {
		event_del(&ev_sighup);
		event_del(&ev_sigchld);
		event_del(&ev_sigcont);
		event_del(&ev_sigterm);
		event_del(&ev_sigusr1);
		event_del(&ev_sigwinch);
	}
}
