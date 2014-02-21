# $Id$

# Obvious program stuff.
bin_PROGRAMS = tmux
CLEANFILES = tmux.1.mdoc tmux.1.man

# Distribution tarball options.
EXTRA_DIST = \
	CHANGES FAQ README TODO COPYING examples compat \
	array.h compat.h tmux.h osdep-*.c mdoc2man.awk tmux.1
dist-hook:
	make clean
	grep "^#found_debug=" configure
	find $(distdir) -name .svn -type d|xargs rm -Rf

# Preprocessor flags.
CPPFLAGS += @XOPEN_DEFINES@ -DTMUX_CONF="\"$(sysconfdir)/tmux.conf\""

# glibc as usual does things ass-backwards and hides useful things by default,
# so everyone has to add this.
if IS_GLIBC
CFLAGS += -D_GNU_SOURCE
endif

# Set flags for gcc. gcc4 whines abouts silly stuff so it needs slightly
# different flags.
if IS_GCC
CFLAGS += -std=gnu99
if IS_DEBUG
CFLAGS += -O0 -g
CFLAGS += -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS += -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CFLAGS += -Wundef -Wbad-function-cast -Winline -Wcast-align
CFLAGS += -Wdeclaration-after-statement
CPPFLAGS += -DDEBUG
else
CFLAGS += -O2
endif
if IS_GCC4
CPPFLAGS += -iquote. -I/usr/local/include
if IS_DEBUG
CFLAGS += -Wno-pointer-sign
endif
else
CPPFLAGS += -I. -I- -I/usr/local/include
endif
endif

# Set flags for Solaris.
if IS_SUNOS
if IS_GCC
CPPFLAGS += -D_XPG6 -D__EXTENSIONS__ -D_POSIX_PTHREAD_SEMANTICS
else
CPPFLAGS += -D_XPG4_2 -D__EXTENSIONS__ -D_POSIX_PTHREAD_SEMANTICS
endif
endif

# Set flags for Sun CC.
if IS_SUNCC
CFLAGS += -erroff=E_EMPTY_DECLARATION
endif

# List of sources.
dist_tmux_SOURCES = \
	arguments.c \
	attributes.c \
	cfg.c \
	client.c \
	clock.c \
	cmd-attach-session.c \
	cmd-bind-key.c \
	cmd-break-pane.c \
	cmd-capture-pane.c \
	cmd-choose-buffer.c \
	cmd-choose-client.c \
	cmd-choose-list.c \
	cmd-choose-tree.c \
	cmd-clear-history.c \
	cmd-clock-mode.c \
	cmd-command-prompt.c \
	cmd-confirm-before.c \
	cmd-copy-mode.c \
	cmd-delete-buffer.c \
	cmd-detach-client.c \
	cmd-display-message.c \
	cmd-display-panes.c \
	cmd-find-window.c \
	cmd-has-session.c \
	cmd-if-shell.c \
	cmd-join-pane.c \
	cmd-kill-pane.c \
	cmd-kill-server.c \
	cmd-kill-session.c \
	cmd-kill-window.c \
	cmd-link-window.c \
	cmd-list-buffers.c \
	cmd-list-clients.c \
	cmd-list-commands.c \
	cmd-list-keys.c \
	cmd-list-panes.c \
	cmd-list-sessions.c \
	cmd-list-windows.c \
	cmd-list.c \
	cmd-load-buffer.c \
	cmd-lock-server.c \
	cmd-move-window.c \
	cmd-new-session.c \
	cmd-new-window.c \
	cmd-paste-buffer.c \
	cmd-pipe-pane.c \
	cmd-queue.c \
	cmd-refresh-client.c \
	cmd-rename-session.c \
	cmd-rename-window.c \
	cmd-resize-pane.c \
	cmd-respawn-pane.c \
	cmd-respawn-window.c \
	cmd-rotate-window.c \
	cmd-run-shell.c \
	cmd-save-buffer.c \
	cmd-select-layout.c \
	cmd-select-pane.c \
	cmd-select-window.c \
	cmd-send-keys.c \
	cmd-set-buffer.c \
	cmd-set-environment.c \
	cmd-set-option.c \
	cmd-show-environment.c \
	cmd-show-messages.c \
	cmd-show-options.c \
	cmd-source-file.c \
	cmd-split-window.c \
	cmd-string.c \
	cmd-suspend-client.c \
	cmd-swap-pane.c \
	cmd-swap-window.c \
	cmd-switch-client.c \
	cmd-unbind-key.c \
	cmd-unlink-window.c \
	cmd-wait-for.c \
	cmd.c \
	colour.c \
	control.c \
	control-notify.c \
	environ.c \
	format.c \
	grid-cell.c \
	grid-view.c \
	grid.c \
	input-keys.c \
	input.c \
	job.c \
	key-bindings.c \
	key-string.c \
	layout-custom.c \
	layout-set.c \
	layout.c \
	log.c \
	mode-key.c \
	names.c \
	notify.c \
	options-table.c \
	options.c \
	paste.c \
	resize.c \
	screen-redraw.c \
	screen-write.c \
	screen.c \
	server-client.c \
	server-fn.c \
	server-window.c \
	server.c \
	session.c \
	signal.c \
	status.c \
	style.c \
	tmux.c \
	tty-acs.c \
	tty-keys.c \
	tty-term.c \
	tty.c \
	utf8.c \
	window-choose.c \
	window-clock.c \
	window-copy.c \
	window.c \
	xmalloc.c \
	xterm-keys.c
nodist_tmux_SOURCES = osdep-@PLATFORM@.c

# Pile in all the compat/ stuff that is needed.
if NO_FORKPTY
nodist_tmux_SOURCES += compat/forkpty-@PLATFORM@.c
endif
if NO_IMSG
nodist_tmux_SOURCES += compat/imsg.c compat/imsg-buffer.c
endif
if NO_CLOSEFROM
nodist_tmux_SOURCES += compat/closefrom.c
endif
if NO_DAEMON
nodist_tmux_SOURCES += compat/daemon.c
endif
if NO_SETENV
nodist_tmux_SOURCES += compat/setenv.c
endif
if NO_STRLCAT
nodist_tmux_SOURCES += compat/strlcat.c
endif
if NO_STRLCPY
nodist_tmux_SOURCES += compat/strlcpy.c
endif
if NO_ASPRINTF
nodist_tmux_SOURCES += compat/asprintf.c
endif
if NO_FGETLN
nodist_tmux_SOURCES += compat/fgetln.c
endif
if NO_GETOPT
nodist_tmux_SOURCES += compat/getopt.c
endif
if NO_STRCASESTR
nodist_tmux_SOURCES += compat/strcasestr.c
endif
if NO_STRSEP
nodist_tmux_SOURCES += compat/strsep.c
endif
if NO_VIS
nodist_tmux_SOURCES += compat/vis.c compat/unvis.c
endif
if NO_STRTONUM
nodist_tmux_SOURCES += compat/strtonum.c
endif
if NO_B64_NTOP
nodist_tmux_SOURCES += compat/b64_ntop.c
endif
if NO_CFMAKERAW
nodist_tmux_SOURCES += compat/cfmakeraw.c
endif
if NO_OPENAT
nodist_tmux_SOURCES += compat/openat.c
endif

# Install tmux.1 in the right format.
install-exec-hook:
	if test x@MANFORMAT@ = xmdoc; then \
		sed -e "s|@SYSCONFDIR@|$(sysconfdir)|g" $(srcdir)/tmux.1 \
			>$(srcdir)/tmux.1.mdoc; \
	else \
		sed -e "s|@SYSCONFDIR@|$(sysconfdir)|g" $(srcdir)/tmux.1| \
			$(AWK) -fmdoc2man.awk >$(srcdir)/tmux.1.man; \
	fi
	$(MKDIR_P) $(DESTDIR)$(mandir)/man1
	$(INSTALL_DATA) $(srcdir)/tmux.1.@MANFORMAT@ \
		$(DESTDIR)$(mandir)/man1/tmux.1

# Update SF web site.
upload-index.html: update-index.html
	scp www/index.html www/main.css www/images/*.png \
		${USER},tmux@web.sf.net:/home/groups/t/tm/tmux/htdocs
	rm -f www/index.html www/images/small-*

update-index.html:
	(cd www/images && \
		rm -f small-* && \
		for i in *.png; do \
			convert "$$i" -resize 200x150 "small-$$i"; \
		done \
	)
	sed "s/%%VERSION%%/${VERSION}/g" www/index.html.in >www/index.html
