# $OpenBSD$

.PATH:	${.CURDIR}/..

PROG=	tmux
SRCS=	arg.c attributes.c buffer-poll.c buffer.c cfg.c client-fn.c \
	client-msg.c client.c clock.c cmd-attach-session.c cmd-bind-key.c \
	cmd-break-pane.c cmd-choose-session.c cmd-choose-window.c \
	cmd-clear-history.c cmd-clock-mode.c cmd-command-prompt.c \
	cmd-confirm-before.c cmd-copy-buffer.c cmd-copy-mode.c \
	cmd-delete-buffer.c cmd-detach-client.c cmd-down-pane.c \
	cmd-find-window.c cmd-generic.c cmd-has-session.c cmd-kill-pane.c \
	cmd-kill-server.c cmd-kill-session.c cmd-kill-window.c \
	cmd-last-window.c cmd-link-window.c cmd-list-buffers.c \
	cmd-list-clients.c cmd-list-commands.c cmd-list-keys.c \
	cmd-list-sessions.c cmd-list-windows.c cmd-list.c cmd-load-buffer.c \
	cmd-lock-server.c cmd-move-window.c cmd-new-session.c cmd-new-window.c \
	cmd-next-layout.c cmd-next-window.c cmd-paste-buffer.c \
	cmd-previous-layout.c cmd-previous-window.c cmd-refresh-client.c \
	cmd-rename-session.c cmd-rename-window.c cmd-resize-pane.c \
	cmd-respawn-window.c cmd-rotate-window.c cmd-save-buffer.c \
	cmd-scroll-mode.c cmd-select-layout.c cmd-select-pane.c \
	cmd-select-prompt.c cmd-select-window.c cmd-send-keys.c \
	cmd-send-prefix.c cmd-server-info.c cmd-set-buffer.c cmd-set-option.c \
	cmd-set-password.c cmd-set-window-option.c cmd-show-buffer.c \
	cmd-show-options.c cmd-show-window-options.c cmd-source-file.c \
	cmd-split-window.c cmd-start-server.c cmd-string.c \
	cmd-suspend-client.c cmd-swap-pane.c cmd-swap-window.c \
	cmd-switch-client.c cmd-unbind-key.c cmd-unlink-window.c \
	cmd-up-pane.c cmd.c colour.c grid-view.c grid.c input-keys.c \
	input.c key-bindings.c key-string.c layout-manual.c layout.c log.c \
	mode-key.c names.c options-cmd.c options.c paste.c procname.c \
	resize.c screen-redraw.c screen-write.c screen.c server-fn.c \
	server-msg.c server.c session.c status.c tmux.c tty-keys.c tty-term.c \
	tty-write.c tty.c utf8.c util.c window-choose.c window-clock.c \
	window-copy.c window-more.c window-scroll.c window.c xmalloc.c

CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align

LDADD=  -lutil -lncurses
DPADD=  ${LIBUTIL}

MAN=	tmux.1

.include <bsd.prog.mk>
