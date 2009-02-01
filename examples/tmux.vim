" Vim syntax file
" Language: tmux(1) configuration file
" Maintainer: Tiago Cunha <me@tiagocunha.org>
" Last Change: $Date: 2009-02-01 18:16:39 $

if version < 600
	syntax clear
elseif exists("b:current_syntax")
	finish
endif

setlocal iskeyword+=-
syntax case match

syn keyword tmuxAction	any current none
syn keyword tmuxBoolean	off on

syn keyword tmuxCmds detach[-client] ls list-sessions neww new-window
syn keyword tmuxCmds bind[-key] unbind[-key] prev[ious-window] last
syn keyword tmuxCmds last[-window] lsk list-keys set[-option] renamew
syn keyword tmuxCmds rename-window selectw select-window lsw list-windows
syn keyword tmuxCmds attach[-session] send-prefix refresh[-client] killw
syn keyword tmuxCmds kill-window lsc list-clients linkw link-window unlinkw
syn keyword tmuxCmds unlink-window next[-window] send[-keys] swapw swap-window
syn keyword tmuxCmds rename[-session] kill-session switchc switch-client
syn keyword tmuxCmds has[-session] scroll-mode copy-mode pasteb paste-buffer
syn keyword tmuxCmds new[-session] start[-server] kill-server setw
syn keyword tmuxCmds set-window-option show[-options] showw show-window-options
syn keyword tmuxCmds command-prompt setb set-buffer showb show-buffer lsb
syn keyword tmuxCmds list-buffers deleteb delete-buffer lscm list-commands
syn keyword tmuxCmds movew move-window select-prompt respawnw respawn-window
syn keyword tmuxCmds source[-file] info server-info clock-mode lock[-server]
syn keyword tmuxCmds pass set-password saveb save-buffer downp down-pane killp
syn keyword tmuxCmds kill-pane resizep-down resize-pane-down resizep-up
syn keyword tmuxCmds resize-pane-up selectp select-pane splitw split-window
syn keyword tmuxCmds upp up-pane choose-session choose-window loadb load-buffer

syn keyword tmuxOptsSet prefix status status-fg status-bg bell-action
syn keyword tmuxOptsSet default-command history-limit status-left status-right
syn keyword tmuxOptsSet status-interval set-titles display-time buffer-limit
syn keyword tmuxOptsSet status-left-length status-right-length message-fg
syn keyword tmuxOptsSet message-bg lock-after-time default-path repeat-time
syn keyword tmuxOptsSet message-attr status-attr

syn keyword tmuxOptsSetw monitor-activity aggressive-resize force-width
syn keyword tmuxOptsSetw force-height remain-on-exit uft8 mode-fg mode-bg
syn keyword tmuxOptsSetw mode-keys clock-mode-colour clock-mode-style
syn keyword tmuxOptsSetw xterm-keys mode-attr

syn keyword tmuxTodo FIXME NOTE TODO XXX contained

syn match tmuxKey		/\(C-\|M-\|\^\)\p/	display
syn match tmuxNumber 		/\d\+/			display
syn match tmuxOptions		/\s-\a/			display
syn match tmuxVariable		/\w\+=/			display
syn match tmuxVariableExpansion	/\${\=\w\+}\=/		display

syn region tmuxComment	start=/#/ end=/$/ contains=tmuxTodo display oneline
syn region tmuxString	start=/"/ end=/"/ display oneline
syn region tmuxString	start=/'/ end=/'/ display oneline

hi def link tmuxAction			Boolean
hi def link tmuxBoolean			Boolean
hi def link tmuxCmds			Keyword
hi def link tmuxComment			Comment
hi def link tmuxKey			Special
hi def link tmuxNumber			Number
hi def link tmuxOptions			Identifier
hi def link tmuxOptsSet			Function
hi def link tmuxOptsSetw		Function
hi def link tmuxString			String
hi def link tmuxTodo			Todo
hi def link tmuxVariable		Constant
hi def link tmuxVariableExpansion	Constant

let b:current_syntax = "tmux"
