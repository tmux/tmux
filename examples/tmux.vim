" Vim syntax file
" Language: tmux(1) configuration file
" Maintainer: Tiago Cunha <tcunha@users.sourceforge.net>
" Last Change: $Date: 2010-07-27 18:29:07 $
" License: This file is placed in the public domain.
"
" To install this file:
"
" - Drop the file in the syntax directory into runtimepath (such as
"  ~/.vim/syntax/tmux.vim).
" - Make the filetype recognisable by adding the following to filetype.vim
"   (~/.vim/filetype.vim):
"
"	augroup filetypedetect
"		au BufNewFile,BufRead .tmux.conf*,tmux.conf* setf tmux
"	augroup END
"
" - Switch on syntax highlighting by adding "syntax enable" to .vimrc.
"

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
	\ attach[-session]
	\ bind[-key]
	\ break-pane
	\ breakp
	\ capture-pane
	\ capturep
	\ choose-buffer
	\ choose-client
	\ choose-list
	\ choose-session
	\ choose-tree
	\ choose-window
	\ clear-history
	\ clearhist
	\ clock-mode
	\ command-prompt
	\ confirm[-before]
	\ copy-mode
	\ delete-buffer
	\ deleteb
	\ detach[-client]
	\ display[-message]
	\ display-panes
	\ displayp
	\ find-window
	\ findw
	\ has[-session]
	\ if[-shell]
	\ join-pane
	\ joinp
	\ kill-pane
	\ killp
	\ kill-server
	\ kill-session
	\ kill-window
	\ killw
	\ last-pane
	\ lastp
	\ last[-window]
	\ link-window
	\ linkw
	\ list-buffers
	\ lsb
	\ list-clients
	\ lsc
	\ list-commands
	\ lscm
	\ list-keys
	\ lsk
	\ list-panes
	\ lsp
	\ list-sessions
	\ ls
	\ list-windows
	\ lsw
	\ load-buffer
	\ loadb
	\ lock-client
	\ lockc
	\ lock[-server]
	\ lock-session
	\ locks
	\ move-pane
	\ movep
	\ move-window
	\ movew
	\ new[-session]
	\ next-layout
	\ nextl
	\ next[-window]
	\ paste-buffer
	\ pasteb
	\ pipe-pane
	\ pipep
	\ previous-layout
	\ prevl
	\ prev[ious-window]
	\ refresh[-client]
	\ rename[-session]
	\ rename-window
	\ renamew
	\ resize-pane
	\ resizep
	\ respawn-pane
	\ respawnp
	\ respawn-window
	\ respawnw
	\ rotate-window
	\ rotatew
	\ run[-shell]
	\ save-buffer
	\ saveb
	\ select-layout
	\ selectl
	\ select-pane
	\ selectp
	\ select-window
	\ selectw
	\ send[-keys]
	\ send-prefix
	\ server-info
	\ info
	\ set-buffer
	\ setb
	\ set-environment
	\ setenv
	\ set[-option]
	\ set-window-option
	\ setw
	\ show-buffer
	\ showb
	\ show-environment
	\ showenv
	\ show-messages
	\ showmsgs
	\ show[-options]
	\ show-window-options
	\ showw
	\ source[-file]
	\ split-window
	\ splitw
	\ start[-server]
	\ suspend-client
	\ suspendc
	\ swap-pane
	\ swapp
	\ swap-window
	\ swapw
	\ switch-client
	\ switchc
	\ unbind[-key]
	\ unlink-window
	\ unlinkw
	\ wait[-for]

syn keyword tmuxOptsSet
	\ assume-paste-time
	\ base-index
	\ bell-action
	\ bell-on-alert
	\ buffer-limit
	\ default-command
	\ default-shell
	\ default-terminal
	\ destroy-unattached
	\ detach-on-destroy
	\ display-panes-active-colour
	\ display-panes-colour
	\ display-panes-time
	\ display-time
	\ escape-time
	\ exit-unattached
	\ focus-events
	\ history-file
	\ history-limit
	\ lock-after-time
	\ lock-command
	\ lock-server
	\ message-command-style
	\ message-limit
	\ message-style
	\ mouse
	\ mouse-utf8
	\ pane-active-border-style
	\ pane-border-style
	\ prefix
	\ prefix2
	\ quiet
	\ renumber-windows
	\ repeat-time
	\ set-clipboard
	\ set-remain-on-exit
	\ set-titles
	\ set-titles-string
	\ status
	\ status-interval
	\ status-justify
	\ status-keys
	\ status-left
	\ status-left-length
	\ status-left-style
	\ status-position
	\ status-right
	\ status-right-length
	\ status-utf8
	\ staus-right-style
	\ terminal-overrides
	\ update-environment
	\ visual-activity
	\ visual-bell
	\ visual-silence
	\ word-separators

syn keyword tmuxOptsSetw
	\ aggressive-resize
	\ allow-rename
	\ alternate-screen
	\ automatic-rename
	\ clock-mode-colour
	\ clock-mode-style
	\ force-height
	\ force-width
	\ main-pane-height
	\ main-pane-width
	\ mode-keys
	\ mode-style
	\ monitor-activity
	\ monitor-silence
	\ other-pane-height
	\ other-pane-width
	\ pane-base-index
	\ remain-on-exit
	\ synchronize-panes
	\ utf8
	\ window-active-style
	\ window-status-activity-style
	\ window-status-bell-style
	\ window-status-current-format
	\ window-status-current-style
	\ window-status-format
	\ window-status-last-style
	\ window-status-separator
	\ window-status-style
	\ window-style
	\ wrap-search
	\ xterm-keys

syn keyword tmuxTodo FIXME NOTE TODO XXX contained

syn match tmuxKey		/\(C-\|M-\|\^\)\+\S\+/	display
syn match tmuxNumber 		/\d\+/			display
syn match tmuxOptions		/\s-\a\+/		display
syn match tmuxVariable		/\w\+=/			display
syn match tmuxVariableExpansion	/\${\=\w\+}\=/		display

" Comments can span multiple lines, when the newline is escaped
" (with a single) backslash at the end.
syn region tmuxComment  start=/#/ skip=/\\\@<!\\$/ end=/$/ contains=tmuxTodo
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
