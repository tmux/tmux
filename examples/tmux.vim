" Vim syntax file
" Language: tmux(1) configuration file
" Maintainer: Tiago Cunha <me@tiagocunha.org>
" Last Change: $Date: 2009-08-31 22:31:44 $

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
syn keyword tmuxCmds bind[-key] unbind[-key] prev[ious-window] last[-window]
syn keyword tmuxCmds lsk list-keys set[-option] renamew rename-window selectw
syn keyword tmuxCmds select-window lsw list-windows attach[-session]
syn keyword tmuxCmds send-prefix refresh[-client] killw kill-window lsc
syn keyword tmuxCmds list-clients linkw link-window unlinkw unlink-window
syn keyword tmuxCmds next[-window] send[-keys] swapw swap-window
syn keyword tmuxCmds rename[-session] kill-session switchc switch-client
syn keyword tmuxCmds has[-session] scroll-mode copy-mode pasteb paste-buffer
syn keyword tmuxCmds new[-session] start[-server] kill-server setw
syn keyword tmuxCmds set-window-option show[-options] showw show-window-options
syn keyword tmuxCmds command-prompt setb set-buffer showb show-buffer lsb
syn keyword tmuxCmds list-buffers deleteb delete-buffer lscm list-commands
syn keyword tmuxCmds movew move-window select-prompt respawnw respawn-window
syn keyword tmuxCmds source[-file] info server-info clock-mode lock[-server]
syn keyword tmuxCmds pass set-password saveb save-buffer downp down-pane killp
syn keyword tmuxCmds kill-pane resizep resize-pane selectp select-pane swapp
syn keyword tmuxCmds swap-pane splitw split-window upp up-pane choose-session
syn keyword tmuxCmds choose-window loadb load-buffer copyb copy-buffer suspendc
syn keyword tmuxCmds suspend-client findw find-window breakp break-pane nextl
syn keyword tmuxCmds next-layout rotatew rotate-window confirm[-before]
syn keyword tmuxCmds clearhist clear-history selectl select-layout if[-shell]
syn keyword tmuxCmds display[-message] set-environment show-environment
syn keyword tmuxCmds choose-client displayp display-panes

syn keyword tmuxOptsSet prefix status status-fg status-bg bell-action
syn keyword tmuxOptsSet default-command history-limit status-left status-right
syn keyword tmuxOptsSet status-interval set-titles display-time buffer-limit
syn keyword tmuxOptsSet status-left-length status-right-length message-fg
syn keyword tmuxOptsSet message-bg lock-after-time default-path repeat-time
syn keyword tmuxOptsSet message-attr status-attr status-keys set-remain-on-exit
syn keyword tmuxOptsSet status-utf8 default-terminal visual-activity
syn keyword tmuxOptsSet visual-bell visual-content status-justify
syn keyword tmuxOptsSet terminal-overrides status-left-attr status-left-bg
syn keyword tmuxOptsSet status-left-fg status-right-attr status-right-bg
syn keyword tmuxOptsSet status-right-fg update-environment base-index
syn keyword tmuxOptsSet display-panes-colour display-panes-time

syn keyword tmuxOptsSetw monitor-activity aggressive-resize force-width
syn keyword tmuxOptsSetw force-height remain-on-exit uft8 mode-fg mode-bg
syn keyword tmuxOptsSetw mode-keys clock-mode-colour clock-mode-style
syn keyword tmuxOptsSetw xterm-keys mode-attr window-status-attr
syn keyword tmuxOptsSetw window-status-bg window-status-fg automatic-rename
syn keyword tmuxOptsSetw main-pane-width main-pane-height monitor-content
syn keyword tmuxOptsSetw window-status-current-attr window-status-current-bg
syn keyword tmuxOptsSetw window-status-current-fg mode-mouse

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
