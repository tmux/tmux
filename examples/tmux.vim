" Vim syntax file
" Language: tmux(1) configuration file
" Maintainer: Tiago Cunha <tcunha@users.sourceforge.net>
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

syn keyword tmuxCmds
	\ attach[-session] detach[-client] has[-session] kill-server
	\ kill-session lsc list-clients lscm list-commands ls list-sessions
	\ lockc lock-client locks lock-session new[-session] refresh[-client]
	\ rename[-session] showmsgs show-messages source[-file] start[-server]
	\ suspendc suspend-client switchc switch-client
	\ copy-mode
	\ breakp break-pane capturep capture-pane choose-client choose-session
	\ choose-tree choose-window displayp display-panes findw find-window
	\ joinp join-pane killp kill-pane killw kill-window lastp last-pane
	\ last[-window] linkw link-window lsp list-panes lsw list-windows movep
	\ move-pane movew move-window neww new-window nextl next-layout
	\ next[-window] pipep pipe-pane prevl previous-layout prev[ious-window]
	\ renamew rename-window resizep resize-pane respawnp respawn-pane
	\ respawnw respawn-window rotatew rotate-window selectl select-layout
	\ selectp select-pane selectw select-window splitw split-window swapp
	\ swap-pane swapw swap-window unlinkw unlink-window
	\ bind[-key] lsk list-keys send[-keys] send-prefix unbind[-key]
	\ set[-option] setw set-window-option show[-options] showw
	\ show-window-options
	\ setenv set-environment showenv show-environment
	\ command-prompt confirm[-before] display[-message]
	\ choose-buffer clearhist clear-history deleteb delete-buffer lsb
	\ list-buffers loadb load-buffer pasteb paste-buffer saveb save-buffer
	\ setb set-buffer showb show-buffer
	\ clock-mode if[-shell] lock[-server] run[-shell] [server-]info
	\ choose-list

syn keyword tmuxOptsSet
	\ buffer-limit escape-time exit-unattached exit-unattached quiet
	\ set-clipboard
	\ base-index bell-action bell-on-alert default-command default-path
	\ default-shell default-terminal destroy-unattached detach-on-destroy
	\ display-panes-[active-]colour display-[panes-]time history-limit
	\ lock-after-time lock-command lock-server message-[command-]attr
	\ message-[command-]bg message-[command-]fg message-limit
	\ mouse-resize-pane mouse-select-pane mouse-select-window mouse-utf8
	\ pane-[active-]border-bg pane-[active-]border-fg prefix prefix2
	\ renumber-windows repeat-time set-remain-on-exit set-titles
	\ set-titles-string status status-attr status-bg status-fg
	\ status-interval status-justify status-keys status-left
	\ status-left-attr status-left-bg status-left-fg status-left-length
	\ status-position status-right status-right-attr status-right-bg
	\ status-right-fg status-right-length status-utf8 terminal-overrides
	\ update-environment visual-activity visual-bell visual-content
	\ visual-silence word-separators

syn keyword tmuxOptsSetw
	\ aggressive-resize alternate-screen automatic-rename
	\ c0-change-interval c0-change-trigger clock-mode-colour
	\ clock-mode-style force-height force-width layout-history-limit
	\ main-pane-height main-pane-width mode-attr mode-bg mode-fg move-keys
	\ mode-mouse monitor-activity monitor-content monitor-silence
	\ other-pane-height other-pane-width pane-base-index remain-on-exit
	\ synchronize-panes utf8 window-status-bell-attr window-status-bell-bg
	\ window-status-bell-fg window-status-content-attr
	\ window-status-content-bg window-status-content-fg
	\ window-status-activity-attr window-status-activity-bg
	\ window-status-activity-fg window-status-attr
	\ window-status-[current-]attr window-status-[current-]bg
	\ window-status-[current-]fg window-status-[current-]format
	\ window-status-separator xterm-keys wrap-search

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
