" Vim syntax file
" Language: tmux(1) configuration file
" Maintainer: Tiago Cunha <me@tiagocunha.org>
" Last Change: $Date: 2008-12-20 09:09:57 $

if version < 600
	syntax clear
elseif exists("b:current_syntax")
	finish
endif

setlocal iskeyword+=-
syntax case match

syn keyword tmuxAction	any current none
syn keyword tmuxBoolean	off on

syn keyword tmuxCmds attach[-session] bind[-key] command-prompt copy-mode
syn keyword tmuxCmds delete-buffer deleteb detach[-client] has[-session]
syn keyword tmuxCmds kill-server kill-session kill-window killw last[-window]
syn keyword tmuxCmds link-window linkw list-buffers lsb list-commands lscm
syn keyword tmuxCmds list-keys lsk list-sessions ls list-windows lsw
syn keyword tmuxCmds move-window movew new[-session] new-window neww
syn keyword tmuxCmds next[-window] paste-buffer pasteb prev[ious-window]
syn keyword tmuxCmds refresh[-client] rename[-session] rename-window renamew
syn keyword tmuxCmds respawn-window respawnw scroll-mode select-prompt
syn keyword tmuxCmds select-window selectw send-keys send-prefix set-buffer
syn keyword tmuxCmds setb set[-option] set-window-option setw show-buffer showb
syn keyword tmuxCmds show[-options] show-window-options showw source[-file]
syn keyword tmuxCmds start-server swap-window swapw switch-client switchc
syn keyword tmuxCmds unbind[-key] unlink-window unlinkw

syn keyword tmuxCmdsSet bell-action buffer-limit default-command display-time
syn keyword tmuxCmdsSet history-limit message-bg message-fg prefix
syn keyword tmuxCmdsSet remain-by-default set-titles status status-bg status-fg
syn keyword tmuxCmdsSet status-interval status-left status-left-length
syn keyword tmuxCmdsSet status-right status-right-length utf8-default

syn keyword tmuxCmdsSetw aggressive-resize force-height force-width mode-bg
syn keyword tmuxCmdsSetw mode-fg mode-keys monitor-activity remain-on-exit utf8

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
hi def link tmuxCmdsSet			Function
hi def link tmuxCmdsSetw		Function
hi def link tmuxComment			Comment
hi def link tmuxKey			Special
hi def link tmuxNumber			Number
hi def link tmuxOptions			Identifier
hi def link tmuxString			String
hi def link tmuxTodo			Todo
hi def link tmuxVariable		Constant
hi def link tmuxVariableExpansion	Constant

let b:current_syntax = "tmux"
