/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "tmux.h"

/*
 * The goal is to create multiplexers for SSH agents, GPG agents, DBus and
 * other such things that are associated with shells that could be multiplexed
 * by tmux. Each comes in two halves: a backing associated with a client that
 * talks to a real server and an offering that is associated with a window for
 * applications to connect to.
 */

struct othermux_class;
struct othermux_connection;
struct othermux_request;

/*
 * Initialise a backing for a client by opening a socket using the path
 * provided. Returns true if the socket is opened, false otherwise.
 */
bool othermux_backing_init(struct othermux_backing *self,
			   struct othermux_class *cls, const char *path,
			   struct client *);
/*
 * Respond to the current requested queue to this backing.
 *
 * To be called only after the backing_request callback has been invoked. The
 * memory management responsibilities of the response are protocol specific.
 */
void othermux_backing_respond(struct othermux_backing *self, void *response);
/*
 * Decrease the reference count on a backing and cleanup if required.
 */
void othermux_backing_unref(struct othermux_backing *self);
/*
 * Remove the backing from its client's of backings.
 */
void othermux_backing_drop(struct othermux_backing *self);

/*
 * Initialse and offering for a window by allocating a socket and putting it in
 * the environment.
 *
 * @self: the structure to initialise
 * @cls: the class definition of the backend
 * @type: a letter to prefix the socket name
 * @variable: the environment variable to set with the socket
 * @w: the window that will own this offering
 * @env: the environment to be mutated to include the socket.
 */
bool othermux_offering_init(struct othermux_offering *self,
			    struct othermux_class *cls, char type,
			    const char *variable, struct window *w,
			    struct environ *env);
void othermux_offering_accept(int fd, short events, void *data);
void othermux_offering_add_accept(struct othermux_offering *self, int timeout);
/*
 * Reduce the reference count on this offering and cleanup if necessary.
 */
void othermux_offering_unref(struct othermux_offering *self);

/*
 * Send a message to all compatible backings and sleep until they respond.
 *
 * The connection will not receive any further messages from the client until
 * the backings have responded. Once all backings have responded, the
 * connection_finished callback will be invoked and the client may process the
 * results.
 *
 * @self: the connection initiating the request
 * @request_data: the message to send. All the recipients will see the same
 * message and the message must persist until the connection_finished callback
 * is invoked.
 */
void othermux_connection_dispatch(struct othermux_connection *self,
				  void *request_data);
/*
 * Explicitly destroy the connection and perform cleanup. This must not be
 * called while backings have outstanding requests.
 */
void othermux_connection_free(struct othermux_connection *self);

/* SSH AGENT */
struct othermux_offering *othermux_ssh_offering_init(struct othermux_class *,
						     struct window *,
						     struct environ *env);
struct othermux_backing *othermux_ssh_backing_init(struct othermux_class *,
						   struct client *,
						   struct environ_entry *);
void othermux_ssh_offering_destroy(struct othermux_offering *);
void othermux_ssh_backing_read(struct othermux_backing *);
void othermux_ssh_backing_request(struct othermux_backing *,
				  struct othermux_request *);
void othermux_ssh_backing_destroy(struct othermux_backing *);
void othermux_ssh_connection_init(struct othermux_connection *);
void othermux_ssh_connection_read(struct othermux_connection *);
void othermux_ssh_connection_finished(struct othermux_connection *);
void othermux_ssh_connection_destroy(struct othermux_connection *);

/* COMMON */
/*
 * The base data structure for an offering. An protocol can create a specific
 * structure with this as the first member if additional fields are required.
 */
struct othermux_offering {
	/* The class/protocol for this offering */
	const struct othermux_class *cls;
	 SLIST_ENTRY(othermux_offering) entry;
	/*
	 * The current reference count. This may be incremented manually, but should
	 * be decremented using othermux_offering_unref.
	 */
	int references;
	/* The path to the socket. */
	char *path;
	/* The file descriptor for the socket. */
	int fd;
	/* The event for incoming connections on the socket. */
	struct event event;
	/* The window that owns this offering. */
	struct window *window;
};

/*
 * An active connection that has been accepted by an offering.
 */
struct othermux_connection {
	/* The number of in-flight requests. Adjusted by othermux_connection_dispatch and othermux_backing_respond. */
	int pending;
	/* The socket connection. */
	struct bufferevent *buffer;
	/* The offering that owns this socket. */
	struct othermux_offering *owner;
	/*
	 * The responses from the last othermux_connection_dispatch. The dispatch
	 * will create a request object for every compatible backing and the backing
	 * will attach its response. These must be processed and freed during
	 * connection_finished and this variable reset to null.
	 */
	 SLIST_HEAD(, othermux_request) requests;
};

/* An in-progress communication from a connection to a backing and back. */
struct othermux_request {
	/* The connection that initiated the message via othermux_connection_dispatch. */
	struct othermux_connection *owner;
	/* The backing receiving the request. */
	struct othermux_backing *target;
	/*
	 * The protocol-specific data in this request. The memory management is
	 * protocol specific and the data is shared by all requests in the same
	 * dispatch.
	 */
	void *request_data;
	/*
	 * The protocol-specific response from the backing. The memory management is
	 * protocol specific.
	 */
	void *response;
	 TAILQ_ENTRY(othermux_request) entry;
	 SLIST_ENTRY(othermux_request) sentry;
};

/* The connection to real service on the tmux client. */
struct othermux_backing {
	/* The class/protocol for this backing */
	const struct othermux_class *cls;
	 SLIST_ENTRY(othermux_backing) entry;
	/*
	 * The current reference count. This may be incremented manually, but should
	 * be decremented using othermux_backing_unref.
	 */
	int references;
	/*
	 * The connection to the real service's socket.
	 */
	struct bufferevent *buffer;
	/*
	 * The client that owns this backing.
	 */
	struct client *client;
	/*
	 * Whether the backing is still on the client's list.
	 */
	bool dropped;
	/*
	 * The queue of requests to be serviced by this backing.
	 */
	 TAILQ_HEAD(, othermux_request) requests;
};

/* SSH */
struct ssh_connection {
	struct othermux_connection base;
	char *current_packet;
};

/* COMMON */
/*
 * The common set of callbacks for protocol. This is also used to match
 * backings and offerings of the same protocol.
 */
struct othermux_class {
	/* The protocol name. */
	const char *name;
	/*
	 * Allocate memory for a new offering, calling othermux_offering_init, and
	 * preparing any protocol-specific fields.
	 *
	 * The returned offering will be added to the supplied window.
	 */
	struct othermux_offering *(*offering_init) (struct othermux_class *,
						    struct window *,
						    struct environ * env);
	/*
	 * Allocate memory for a new backing, calling othermux_backing_init, and
	 * preparing and protocol-specific fields.
	 *
	 * The returned backing will be added to the supplied client.
	 */
	struct othermux_backing *(*backing_init) (struct othermux_class *,
						  struct client *,
						  struct environ_entry *);
	/*
	 * Clean up any protocol-specific fields in this offering. Do not free the structure.
	 */
	void (*offering_destroy) (struct othermux_offering *);
	/*
	 * Called when data is available to read on the socket. This callback may
	 * return doing nothing (waiting for more data) or call
	 * othermux_backing_respond. If a socket error or EOF occurs, this will not be
	 * called.
	 */
	void (*backing_read) (struct othermux_backing *);
	/*
	 * Called to process a request from a connection. The request will always be
	 * accessible via TAILQ_HEAD(&self->requests) until othermux_backing_respond is
	 * called. At which point, the next message will be called.
	 */
	void (*backing_request) (struct othermux_backing *,
				 struct othermux_request *);
	/*
	 * Clean up any protocol-specific data.
	 */
	void (*backing_destroy) (struct othermux_backing *);
	/*
	 * The size to allocate for a connection data structure. This must be at least sizeof(struct othermux_connection).
	 */
	size_t connection_size;
	/*
	 * Initialise any protocol-specific fields in a connection. The common fields
	 * will be initialised before this is called.
	 */
	void (*connection_init) (struct othermux_connection *);
	/*
	 * A callback invoked when data is ready to read on the socket.
	 */
	void (*connection_read) (struct othermux_connection *);
	/*
	 * Called when all the requests sent via dispatch have been completed.
	 *
	 * If an error occurred on the socket, the buffer may be null. The connection
	 * should still process and free any requests.
	 */
	void (*connection_finished) (struct othermux_connection *);
	/*
	 * Clean up any protocol-specific fields in a connection.
	 */
	void (*connection_destroy) (struct othermux_connection *);
};

struct othermux_class othermux_classes[] = {
	{
	 "ssh-agent",
	 othermux_ssh_offering_init,
	 othermux_ssh_backing_init,
	 othermux_ssh_offering_destroy,
	 othermux_ssh_backing_read,
	 othermux_ssh_backing_request,
	 othermux_ssh_backing_destroy,
	 sizeof(struct ssh_connection),
	 othermux_ssh_connection_init,
	 othermux_ssh_connection_read,
	 othermux_ssh_connection_finished,
	 othermux_ssh_connection_destroy}
};

void othermux_connection_dispatch(struct othermux_connection *self,
				  void *request_data)
{
	struct client *c;
	struct othermux_backing *b;
	log_debug("othermux/%s dispatch", self->owner->cls->name);
	self->pending = 1;
	TAILQ_FOREACH(c, &clients, entry) {
		if (winlink_find_by_window_id
		    (&c->session->windows, self->owner->window->id) != NULL) {
			SLIST_FOREACH(b, &c->backings, entry) {
				if (b->cls == self->owner->cls) {
					struct othermux_request *request =
					    xmalloc(sizeof
						    (struct othermux_request));
					bool mustinvoke =
					    TAILQ_EMPTY(&b->requests);
					self->pending++;
					request->target = b;
					request->owner = self;
					request->request_data = request_data;
					SLIST_INSERT_HEAD(&self->requests,
							  request, sentry);
					TAILQ_INSERT_TAIL(&b->requests,
							  request, entry);
					if (mustinvoke) {
						log_debug
						    ("othermux/%s delivering request to %p",
						     b->cls->name, b->client);
						b->references++;
						b->cls->backing_request(b,
									request);
					} else {
						log_debug
						    ("othermux/%s queueing request to %p",
						     b->cls->name, b->client);
					}
				}
			}
		}
	}
	if (--self->pending == 0) {
		size_t size;
		log_debug("othermux/%s dispatch finished",
			  self->owner->cls->name);
		self->owner->cls->connection_finished(self);
		if (self->buffer == NULL) {
			othermux_connection_free(self);
		} else
		    if ((size =
			 evbuffer_get_length(bufferevent_get_input
					     (self->buffer))) > 0) {
			log_debug("othermux/%s %zu more bytes in buffer",
				  self->owner->cls->name, size);
			self->owner->cls->connection_read(self);
		}
	}
}

void othermux_add_window(struct window *w, struct environ *env)
{
	size_t i;
	log_debug("othermux window %u added", w->id);
	SLIST_INIT(&w->offerings);
	for (i = 0; i < sizeof(othermux_classes) / sizeof(*othermux_classes);
	     i++) {
		struct othermux_offering *offering =
		    othermux_classes[i].offering_init(&othermux_classes[i], w,
						      env);
		if (offering != NULL) {
			log_debug("othermux/%s window %u added",
				  othermux_classes[i].name, w->id);
			SLIST_INSERT_HEAD(&w->offerings, offering, entry);
		}
	}
}

void othermux_remove_window(struct window *w)
{
	struct othermux_offering *offering;
	struct othermux_offering *temp;
	log_debug("othermux window %u removed", w->id);
	SLIST_FOREACH_SAFE(offering, &w->offerings, entry, temp) {
		othermux_offering_unref(offering);
	}
}

void othermux_add_client(struct client *c, struct environ_entry *entry)
{
	size_t i;
	if (entry == NULL) {
		return;
	}
	for (i = 0; i < sizeof(othermux_classes) / sizeof(*othermux_classes);
	     i++) {
		struct othermux_backing *backing =
		    othermux_classes[i].backing_init(&othermux_classes[i], c,
						     entry);
		if (backing != NULL) {
			log_debug("othermux/%s backing %p added",
				  othermux_classes[i].name, c);
			SLIST_INSERT_HEAD(&c->backings, backing, entry);
		}
	}
}

void othermux_remove_client(struct client *c)
{
	struct othermux_backing *backing;
	struct othermux_backing *temp;
	log_debug("othermux client %p removed", c);
	SLIST_FOREACH_SAFE(backing, &c->backings, entry, temp) {
		backing->dropped = true;
		othermux_backing_unref(backing);
	}
}

static void connection_eventcb(struct bufferevent *buffer, short events,
			       void *ptr)
{
	struct othermux_connection *self = (struct othermux_connection *)ptr;

	if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
		bufferevent_free(buffer);
		self->buffer = NULL;
		if (SLIST_EMPTY(&self->requests)) {
			othermux_connection_free(self);
		}
	}
}

static void connection_readcb(__unused struct bufferevent *bev, void *ptr)
{
	struct othermux_connection *self = (struct othermux_connection *)ptr;

	if (self->pending == 0
	    && evbuffer_get_length(bufferevent_get_input(self->buffer)) > 0) {
		self->owner->cls->connection_read(self);
	}
}

void othermux_offering_accept(int fd, short events, void *data)
{
	struct othermux_offering *self = (struct othermux_offering *)data;
	struct othermux_connection *conn;
	int newfd;

	othermux_offering_add_accept(self, 0);
	if (!(events & EV_READ))
		return;

	newfd = accept(fd, NULL, NULL);
	if (newfd == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED)
			return;
		if (errno == ENFILE || errno == EMFILE) {
			/* Delete and don't try again for 1 second. */
			othermux_offering_add_accept(self, 1);
			return;
		}
		log_debug("othermux/%s failed to accept for %u: %s",
			  self->cls->name, self->window->id, strerror(errno));
		return;
	}
	log_debug("othermux/%s accept connection %d for %u: %s",
		  self->cls->name, newfd, self->window->id, strerror(errno));
	evutil_make_socket_nonblocking(newfd);
	fcntl(newfd, F_SETFD, FD_CLOEXEC);
	self->references++;
	conn = xmalloc(self->cls->connection_size);
	conn->pending = 0;
	conn->buffer =
	    bufferevent_socket_new(event_get_base(&self->event), newfd,
				   BEV_OPT_CLOSE_ON_FREE |
				   BEV_OPT_DEFER_CALLBACKS);
	if (conn->buffer == NULL) {
		free(conn);
		close(newfd);
		return;
	}
	conn->owner = self;
	SLIST_INIT(&conn->requests);
	self->cls->connection_init(conn);
	bufferevent_setcb(conn->buffer, connection_readcb, NULL,
			  connection_eventcb, conn);
	bufferevent_setwatermark(conn->buffer, EV_READ, 1, 0);
	bufferevent_enable(conn->buffer, EV_READ);
}

void othermux_offering_unref(struct othermux_offering *self)
{
	if (--self->references == 0) {
		self->cls->offering_destroy(self);
		if (event_initialized(&self->event))
			event_del(&self->event);
		if (self->fd >= 0)
			close(self->fd);
		unlink(self->path);
		free(self->path);
		free(self);
	}
}

void othermux_connection_free(struct othermux_connection *self)
{
	self->owner->cls->connection_destroy(self);
	if (self->buffer != NULL)
		bufferevent_free(self->buffer);
	othermux_offering_unref(self->owner);
	free(self);
}

void othermux_offering_add_accept(struct othermux_offering *self, int timeout)
{
	struct timeval tv = { timeout, 0 };

	if (event_initialized(&self->event))
		event_del(&self->event);

	if (timeout == 0) {
		event_set(&self->event, self->fd, EV_READ,
			  othermux_offering_accept, self);
		event_add(&self->event, NULL);
	} else {
		event_set(&self->event, self->fd, EV_TIMEOUT,
			  othermux_offering_accept, self);
		event_add(&self->event, &tv);
	}
}

bool othermux_offering_init(struct othermux_offering *self,
			    struct othermux_class *cls, char type,
			    const char *variable, struct window *w,
			    struct environ *env)
{
#ifdef PF_UNIX
	struct sockaddr_un addr;
	char filename[200];
	struct stat st;

	self->cls = cls;
	self->references = 1;
	self->window = w;
	self->fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (self->fd < 0) {
		log_debug("othermux/%s failed to allocate socket for %u: %s",
			  cls->name, w->id, strerror(errno));
		return false;
	}
	evutil_make_socket_nonblocking(self->fd);

	snprintf(filename, sizeof filename, "%c%u", type, w->id);
	self->path = make_label(filename);
	if (self->path == NULL) {
		log_debug("othermux/%s failed to get socket path for %u: %s",
			  cls->name, w->id, strerror(errno));
		close(self->fd);
		return false;
	}
	fcntl(self->fd, F_SETFD, FD_CLOEXEC);
	if (stat(self->path, &st) >= 0) {
		if (!S_ISSOCK(st.st_mode)) {
			log_debug("othermux/%s path %s is in use for %u: %s",
				  cls->name, self->path, w->id,
				  strerror(errno));
			free(self->path);
			close(self->fd);
			return false;
		}

		unlink(self->path);
	}
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, self->path, sizeof addr.sun_path);
	addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
	if (bind(self->fd, (struct sockaddr *)(&addr), SUN_LEN(&addr)) != 0) {
		log_debug("othermux/%s failed to bind socket for %u on %s: %s",
			  cls->name, w->id, self->path, strerror(errno));
		close(self->fd);
		free(self->path);
		return false;
	}
	if (listen(self->fd, 5) != 0) {
		log_debug("othermux/%s failed to listen on socket for %u: %s",
			  cls->name, w->id, strerror(errno));
		close(self->fd);
		free(self->path);
		return false;
	}
	memset(&self->event, 0, sizeof(struct event));
	event_set(&self->event, self->fd, EV_READ, othermux_offering_accept,
		  self);
	event_add(&self->event, NULL);
	environ_set(env, variable, "%s", self->path);
	return true;
#else
	return false;
#endif
}

static void backing_eventcb(struct bufferevent *buffer, short events, void *ptr)
{
	struct othermux_backing *self = (struct othermux_backing *)ptr;

	if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
		bufferevent_free(buffer);
		self->buffer = NULL;
		self->references++;
		othermux_backing_drop(self);
		if (!TAILQ_EMPTY(&self->requests)) {
			self->cls->backing_read(self);
		}
		othermux_backing_unref(self);
	}
}

static void backing_readcb(__unused struct bufferevent *bev, void *ptr)
{
	struct othermux_backing *self = (struct othermux_backing *)ptr;
	log_debug("othermux/%s data read for backing for client %p",
		  self->cls->name, self->client);
	self->cls->backing_read(self);
}

bool othermux_backing_init(struct othermux_backing *self,
			   struct othermux_class *cls, const char *path,
			   struct client *c)
{
#ifdef PF_UNIX
	int fd;
	struct sockaddr_un addr;
	self->cls = cls;
	self->client = c;
	self->dropped = false;
	self->references = 1;
	TAILQ_INIT(&self->requests);
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		log_debug("othermux/%s failed to allocate socket for %p: %s",
			  cls->name, c, strerror(errno));
		return false;
	}
	evutil_make_socket_nonblocking(fd);
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof addr.sun_path);
	addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
	if (connect(fd, (struct sockaddr *)(&addr), SUN_LEN(&addr)) != 0) {
		log_debug
		    ("othermux/%s failed to connect to socket %s for %p: %s",
		     cls->name, path, c, strerror(errno));
		close(fd);
		return false;
	}
	self->buffer =
	    bufferevent_socket_new(event_get_base(&c->event), fd,
				   BEV_OPT_CLOSE_ON_FREE |
				   BEV_OPT_DEFER_CALLBACKS);
	if (self->buffer == NULL) {
		return false;
	}
	bufferevent_setcb(self->buffer, backing_readcb, NULL, backing_eventcb,
			  self);
	bufferevent_setwatermark(self->buffer, EV_READ, 1, 0);
	bufferevent_enable(self->buffer, EV_READ);
	return true;
#else
	return false;
#endif
}

void othermux_backing_respond(struct othermux_backing *self, void *response)
{
	struct othermux_request *request = TAILQ_FIRST(&self->requests);
	if (request == NULL) {
		log_debug
		    ("othermux/%s backing for %p responding to non-existent request",
		     self->cls->name, self->client);
	}
	request->response = response;
	TAILQ_REMOVE(&self->requests, request, entry);
	if (--request->owner->pending == 0) {
		struct othermux_connection *connection = request->owner;
		log_debug("othermux/%s reponse by %p causes finish",
			  self->cls->name, self->client);
		self->cls->connection_finished(connection);
		if (self->buffer == NULL) {
			log_debug("othermux/%s connection is now dead",
				  self->cls->name);
			othermux_connection_free(connection);
		} else {
			log_debug("othermux/%s connection read again",
				  self->cls->name);
			self->cls->connection_read(connection);
		}
	}
	if (TAILQ_EMPTY(&self->requests)) {
		log_debug("othermux/%s backing for %p is idle", self->cls->name,
			  self->client);
		othermux_backing_unref(self);
	} else {
		log_debug
		    ("othermux/%s backing for %p processing queued request",
		     self->cls->name, self->client);
		self->cls->backing_request(self, TAILQ_FIRST(&self->requests));
	}
}

void othermux_backing_unref(struct othermux_backing *self)
{
	if (--self->references == 0) {
		log_debug("othermux/%s destroying backing for client %p",
			  self->cls->name, self->client);
		self->cls->backing_destroy(self);
		if (self->buffer != NULL)
			bufferevent_free(self->buffer);
		free(self);
	}
}

void othermux_backing_drop(struct othermux_backing *self)
{
	if (self->dropped) {
		return;
	}
	self->dropped = true;
	SLIST_REMOVE(&self->client->backings, self, othermux_backing, entry);
	othermux_backing_unref(self);
}

/* SSH-Agent: https://github.com/openssh/openssh-portable/blob/master/PROTOCOL.agent */
#define SSH_AGENTC_REQUEST_RSA_IDENTITIES 1
#define SSH_AGENTC_RSA_CHALLENGE 3
#define SSH_AGENTC_ADD_RSA_IDENTITY 7
#define SSH_AGENTC_REMOVE_RSA_IDENTITY 8
#define SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES 9
#define SSH_AGENTC_ADD_RSA_ID_CONSTRAINED 24
#define SSH2_AGENTC_REQUEST_IDENTITIES 11
#define SSH2_AGENTC_SIGN_REQUEST 13
#define SSH2_AGENTC_ADD_IDENTITY 17
#define SSH2_AGENTC_REMOVE_IDENTITY 18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES 19
#define SSH2_AGENTC_ADD_ID_CONSTRAINED 25
#define SSH_AGENTC_ADD_SMARTCARD_KEY 20
#define SSH_AGENTC_REMOVE_SMARTCARD_KEY 21
#define SSH_AGENTC_LOCK 22
#define SSH_AGENTC_UNLOCK 23
#define SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED 26
#define SSH_AGENT_FAILURE 5
#define SSH_AGENT_SUCCESS 6
#define SSH_AGENT_RSA_IDENTITIES_ANSWER 2
#define SSH_AGENT_RSA_RESPONSE 4
#define SSH2_AGENT_IDENTITIES_ANSWER 12
#define SSH2_AGENT_SIGN_RESPONSE 14
#define SSH_AGENT_CONSTRAIN_LIFETIME 1
#define SSH_AGENT_CONSTRAIN_CONFIRM 2

struct ssh_offering {
	struct othermux_offering base;
	char *password;
	size_t password_length;
};

struct ssh_backing {
	struct othermux_backing base;
	char *current_packet;
};

struct othermux_offering *othermux_ssh_offering_init(struct othermux_class *cls,
						     struct window *w,
						     struct environ *env)
{
	struct ssh_offering *self = xmalloc(sizeof(struct ssh_offering));
	if (othermux_offering_init
	    (&self->base, cls, 's', "SSH_AUTH_SOCK", w, env)) {
		self->password = NULL;
		self->password_length = 0;
		return &self->base;
	}
	free(self);
	return NULL;
}

struct othermux_backing *othermux_ssh_backing_init(struct othermux_class *cls,
						   struct client *c,
						   struct environ_entry *entry)
{
	struct ssh_backing *self;
	if (strcmp(entry->name, "SSH_AUTH_SOCK") != 0) {
		return NULL;
	}
	log_debug("othermux/%s found environment variable for %p",
		  cls->name, c);
	self = xmalloc(sizeof(struct ssh_backing));
	if (othermux_backing_init(&self->base, cls, entry->value, c)) {
		self->current_packet = NULL;
		return &self->base;
	}
	free(self);
	return NULL;
}

void othermux_ssh_offering_destroy(struct othermux_offering *self)
{
	struct ssh_offering *real_self = (struct ssh_offering *)self;
	if (real_self->password != NULL) {
		free(real_self->password);
	}
}

void othermux_ssh_backing_read(struct othermux_backing *self)
{
	struct ssh_backing *real_self = (struct ssh_backing *)self;
	struct evbuffer *input = bufferevent_get_input(self->buffer);
	uint32_t nsize;
	uint32_t hsize;
	char *response;
	if (evbuffer_copyout(input, &nsize, 4) < 4) {
		return;
	}
	hsize = ntohl(nsize);
	if (real_self->current_packet == NULL) {
		real_self->current_packet = xmalloc(hsize + 4);
	}
	if (evbuffer_copyout(input, real_self->current_packet, hsize + 4) <
	    hsize + 4) {
		return;
	}
	evbuffer_drain(input, hsize + 4);
	response = real_self->current_packet;
	real_self->current_packet = NULL;
	othermux_backing_respond(self, response);
}

void othermux_ssh_backing_request(struct othermux_backing *self,
				  struct othermux_request *request)
{
	uint32_t size = ntohl(*((uint32_t *) request->request_data));
	if (self->buffer == NULL) {
		othermux_backing_respond(self, NULL);
	}
	bufferevent_write(self->buffer, request->request_data, size + 4);
}

void othermux_ssh_backing_destroy(__unused struct othermux_backing *self)
{
}

void othermux_ssh_connection_init(__unused struct othermux_connection *self)
{
	struct ssh_connection *real_self = (struct ssh_connection *)self;
	real_self->current_packet = NULL;
}
static char ssh_failure[] = { 0, 0, 0, 1, SSH_AGENT_FAILURE };
static char ssh_success[] = { 0, 0, 0, 1, SSH_AGENT_SUCCESS };

void othermux_ssh_connection_read(struct othermux_connection *self)
{
	struct ssh_connection *real_self = (struct ssh_connection *)self;
	struct ssh_offering *owner = (struct ssh_offering *)self->owner;
	struct evbuffer *input = bufferevent_get_input(self->buffer);
	uint32_t nsize;
	uint32_t hsize;
	if (evbuffer_copyout(input, &nsize, 4) < 4) {
		return;
	}
	hsize = ntohl(nsize);
	if (real_self->current_packet == NULL) {
		real_self->current_packet = xmalloc(hsize + 4);
	}
	if (evbuffer_copyout(input, real_self->current_packet, hsize + 4) <
	    hsize + 4) {
		return;
	}
	if (hsize < 1) {
		othermux_connection_free(self);
		return;
	}
	evbuffer_drain(input, hsize + 4);
	if (real_self->current_packet[4] == SSH_AGENTC_UNLOCK
	    || real_self->current_packet[4] == SSH_AGENTC_LOCK) {
		union {
			uint32_t i;
			char c[4];
		} unaligned_int;
		size_t password_length;
		if ((real_self->current_packet[4] == SSH_AGENTC_UNLOCK
		     && owner->password == NULL)
		    || (real_self->current_packet[4] == SSH_AGENTC_LOCK
			&& owner->password != NULL)) {
			bufferevent_write(self->buffer, ssh_failure, 5);
			free(real_self->current_packet);
			real_self->current_packet = NULL;
			return;
		}
		memcpy(unaligned_int.c, real_self->current_packet + 5, 4);
		password_length = ntohl(unaligned_int.i);
		if (real_self->current_packet[4] == SSH_AGENTC_UNLOCK) {
			if (password_length == owner->password_length
			    && memcmp(real_self->current_packet + 9,
				      owner->password, password_length) == 0) {
				free(owner->password);
				owner->password = NULL;
				owner->password_length = 0;
				bufferevent_write(self->buffer, ssh_success, 5);
			} else {
				bufferevent_write(self->buffer, ssh_failure, 5);
			}
		} else {
			owner->password = xmalloc(password_length);
			memcpy(owner->password, real_self->current_packet + 9,
			       password_length);
			owner->password_length = password_length;
			bufferevent_write(self->buffer, ssh_success, 5);
		}
		free(real_self->current_packet);
		real_self->current_packet = NULL;
		return;
	}
	if (owner->password != NULL) {
		bufferevent_write(self->buffer, ssh_failure, 5);
		free(real_self->current_packet);
		real_self->current_packet = NULL;
		return;
	}
	othermux_connection_dispatch(self, real_self->current_packet);
}

void othermux_ssh_connection_finished(struct othermux_connection *self)
{
	struct ssh_connection *real_self = (struct ssh_connection *)self;
	struct othermux_request *request, *temp;
	struct evbuffer *output;
	bool success;
	uint32_t size;
	uint32_t total_size;
	uint32_t count;
	char cmd;
	switch (real_self->current_packet[4]) {
		/* First successful answer. */
	case SSH2_AGENTC_SIGN_REQUEST:
	case SSH_AGENTC_RSA_CHALLENGE:
		success = false;
		SLIST_FOREACH_SAFE(request, &self->requests, sentry, temp) {
			if (!success && request->response != NULL
			    && (size =
				ntohl(*((uint32_t *) request->response))) > 4
			    && ((char *)request->response)[4] !=
			    SSH_AGENT_FAILURE) {
				bufferevent_write(self->buffer,
						  request->response, size);
				success = true;
			}
			if (request->response != NULL) {
				free(request->response);
			}
			free(request);
		}
		if (!success) {
			bufferevent_write(self->buffer, ssh_failure, 5);
		}
		break;
		/* Succeed if any backing succeeded. */
	case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
	case SSH2_AGENTC_REMOVE_IDENTITY:
	case SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
	case SSH_AGENTC_REMOVE_RSA_IDENTITY:
	case SSH_AGENTC_REMOVE_SMARTCARD_KEY:
		success = SLIST_EMPTY(&self->requests);
		SLIST_FOREACH_SAFE(request, &self->requests, sentry, temp) {
			if (!success && request->response != NULL
			    && ntohl(*((uint32_t *) request->response)) > 0
			    && ((char *)request->response)[4] ==
			    SSH_AGENT_SUCCESS) {
				success = true;
			}
			if (request->response != NULL) {
				free(request->response);
			}
			free(request);
		}
		bufferevent_write(self->buffer,
				  success ? ssh_success : ssh_failure, 5);
		break;
		/* Succeed if all backings succeeded. */
	case SSH2_AGENTC_ADD_IDENTITY:
	case SSH2_AGENTC_ADD_ID_CONSTRAINED:
	case SSH_AGENTC_ADD_RSA_IDENTITY:
	case SSH_AGENTC_ADD_RSA_ID_CONSTRAINED:
	case SSH_AGENTC_ADD_SMARTCARD_KEY:
	case SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED:
		success = true;
		SLIST_FOREACH_SAFE(request, &self->requests, sentry, temp) {
			if (success
			    && (request->response == NULL
				|| ntohl(*((uint32_t *) request->response)) < 1
				|| ((char *)request->response)[4] ==
				SSH_AGENT_FAILURE)) {
				success = false;
			}
			if (request->response != NULL) {
				free(request->response);
			}
			free(request);
		}
		bufferevent_write(self->buffer,
				  success ? ssh_success : ssh_failure, 5);
		break;
		/* Merge results. */
	case SSH2_AGENTC_REQUEST_IDENTITIES:
	case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
		total_size = 5;
		count = 0;
		SLIST_FOREACH_SAFE(request, &self->requests, sentry, temp) {
			if (request->response != NULL
			    && (size =
				ntohl(*((uint32_t *) request->response))) > 5) {
				if (((char *)request->response)[4] ==
				    SSH_AGENT_RSA_IDENTITIES_ANSWER
				    || ((char *)request->response)[4] ==
				    SSH2_AGENT_IDENTITIES_ANSWER) {
					union {
						uint32_t i;
						char c[4];
					} unaligned_int;
					total_size += size - 5;
					memcpy(unaligned_int.c,
					       ((char *)request->response) + 5,
					       4);
					count += ntohl(unaligned_int.i);
				}
			}
		}
		output = bufferevent_get_output(self->buffer);
		total_size = htonl(total_size);
		evbuffer_add(output, &total_size, 4);
		cmd =
		    (real_self->current_packet[4] ==
		     SSH2_AGENTC_REQUEST_IDENTITIES) ?
		    SSH2_AGENT_IDENTITIES_ANSWER :
		    SSH_AGENT_RSA_IDENTITIES_ANSWER;
		evbuffer_add(output, &cmd, 1);
		count = htonl(count);
		evbuffer_add(output, &count, 4);

		SLIST_FOREACH_SAFE(request, &self->requests, sentry, temp) {
			if (request->response != NULL
			    && (size =
				ntohl(*((uint32_t *) request->response))) > 5
			    && (((char *)request->response)[4] ==
				SSH_AGENT_RSA_IDENTITIES_ANSWER
				|| ((char *)request->response)[4] ==
				SSH2_AGENT_IDENTITIES_ANSWER)) {
				evbuffer_add(output,
					     ((char *)request->response) + 9,
					     size - 5);
			}
			if (request->response != NULL) {
				free(request->response);
			}
			free(request);
		}
		break;
	default:
		bufferevent_write(self->buffer, ssh_failure, 5);
	}
	SLIST_INIT(&self->requests);
	free(real_self->current_packet);
	real_self->current_packet = NULL;
}

void othermux_ssh_connection_destroy(struct othermux_connection *self)
{
	struct ssh_connection *real_self = (struct ssh_connection *)self;
	if (real_self->current_packet != NULL) {
		free(real_self->current_packet);
	}
}
