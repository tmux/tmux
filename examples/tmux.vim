" Vim syntax file
" Language: tmux(1) configuration file
" Maintainer: Tiago Cunha <me@tiagocunha.org>
" Last Change: $Date: 2010-07-27 18:29:07 $
" License: This file is placed in the public domain.

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
syn keyword tmuxCmds has[-session] copy-mode pasteb paste-buffer
syn keyword tmuxCmds new[-session] start[-server] kill-server setw
syn keyword tmuxCmds set-window-option show[-options] showw show-window-options
syn keyword tmuxCmds command-prompt setb set-buffer showb show-buffer lsb
syn keyword tmuxCmds list-buffers deleteb delete-buffer lscm list-commands
syn keyword tmuxCmds movew move-window respawnw respawn-window
syn keyword tmuxCmds source[-file] info server-info clock-mode lock[-server]
syn keyword tmuxCmds saveb save-buffer killp
syn keyword tmuxCmds kill-pane resizep resize-pane selectp select-pane swapp
syn keyword tmuxCmds swap-pane splitw split-window choose-session
syn keyword tmuxCmds choose-window loadb load-buffer copyb copy-buffer suspendc
syn keyword tmuxCmds suspend-client findw find-window breakp break-pane nextl
syn keyword tmuxCmds next-layout rotatew rotate-window confirm[-before]
syn keyword tmuxCmds clearhist clear-history selectl select-layout if[-shell]
syn keyword tmuxCmds display[-message] setenv set-environment showenv
syn keyword tmuxCmds show-environment choose-client displayp display-panes
syn keyword tmuxCmds run[-shell] lockc lock-client locks lock-session lsp
syn keyword tmuxCmds list-panes pipep pipe-pane showmsgs show-messages capturep
syn keyword tmuxCmds capture-pane joinp join-pane choose-buffer

syn keyword tmuxOptsSet prefix status status-fg status-bg bell-action
syn keyword tmuxOptsSet default-command history-limit status-left status-right
syn keyword tmuxOptsSet status-interval set-titles display-time buffer-limit
syn keyword tmuxOptsSet status-left-length status-right-length
syn keyword tmuxOptsSet message-[command-]bg lock-after-time default-path
syn keyword tmuxOptsSet message-[command-]attr status-attr set-remain-on-exit
syn keyword tmuxOptsSet status-utf8 default-terminal visual-activity repeat-time
syn keyword tmuxOptsSet visual-bell visual-content status-justify status-keys
syn keyword tmuxOptsSet terminal-overrides status-left-attr status-left-bg
syn keyword tmuxOptsSet status-left-fg status-right-attr status-right-bg
syn keyword tmuxOptsSet status-right-fg update-environment base-index
syn keyword tmuxOptsSet display-panes-colour display-panes-time default-shell
syn keyword tmuxOptsSet set-titles-string lock-command lock-server
syn keyword tmuxOptsSet mouse-select-pane message-limit quiet escape-time
syn keyword tmuxOptsSet pane-active-border-bg pane-active-border-fg
syn keyword tmuxOptsSet pane-border-bg pane-border-fg message-[command-]fg
syn keyword tmuxOptsSet display-panes-active-colour alternate-screen
syn keyword tmuxOptsSet detach-on-destroy word-separators
syn keyword tmuxOptsSet destroy-unattached exit-unattached set-clipboard
syn keyword tmuxOptsSet bell-on-alert mouse-select-window mouse-utf8

syn keyword tmuxOptsSetw monitor-activity aggressive-resize force-width
syn keyword tmuxOptsSetw force-height remain-on-exit uft8 mode-fg mode-bg
syn keyword tmuxOptsSetw mode-keys clock-mode-colour clock-mode-style
syn keyword tmuxOptsSetw xterm-keys mode-attr window-status-attr
syn keyword tmuxOptsSetw window-status-bg window-status-fg automatic-rename
syn keyword tmuxOptsSetw main-pane-width main-pane-height monitor-content
syn keyword tmuxOptsSetw window-status-current-attr window-status-current-bg
syn keyword tmuxOptsSetw window-status-current-fg mode-mouse synchronize-panes
syn keyword tmuxOptsSetw window-status-format window-status-current-format
syn keyword tmuxOptsSetw window-status-alert-attr
syn keyword tmuxOptsSetw window-status-alert-bg window-status-alert-fg
syn keyword tmuxOptsSetw pane-base-index other-pane-height other-pane-width

syn keyword tmuxTodo FIXME NOTE TODO XXX contained

syn match tmuxKey		/\(C-\|M-\|\^\)\+\S\+/	display
syn match tmuxNumber 		/\d\+/			display
syn match tmuxOptions		/\s-\a\+/		display
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
