" tmux.vim - Set xterm input codes passed by tmux
" Author:        Mark Oteiza
" License:       Public domain
" Description:   Simple plugin that assigns some xterm(1)-style keys to escape
" sequences passed by tmux when "xterm-keys" is set to "on".  Inspired by an
" example given by Chris Johnsen at:
"     https://stackoverflow.com/a/15471820
"
" Documentation: help:xterm-modifier-keys man:tmux(1)

if exists("g:loaded_tmux") || &cp
  finish
endif
let g:loaded_tmux = 1

function! s:SetXtermCapabilities()
  set ttymouse=sgr

  execute "set <xUp>=\e[1;*A"
  execute "set <xDown>=\e[1;*B"
  execute "set <xRight>=\e[1;*C"
  execute "set <xLeft>=\e[1;*D"

  execute "set <xHome>=\e[1;*H"
  execute "set <xEnd>=\e[1;*F"

  execute "set <Insert>=\e[2;*~"
  execute "set <Delete>=\e[3;*~"
  execute "set <PageUp>=\e[5;*~"
  execute "set <PageDown>=\e[6;*~"

  execute "set <xF1>=\e[1;*P"
  execute "set <xF2>=\e[1;*Q"
  execute "set <xF3>=\e[1;*R"
  execute "set <xF4>=\e[1;*S"

  execute "set <F5>=\e[15;*~"
  execute "set <F6>=\e[17;*~"
  execute "set <F7>=\e[18;*~"
  execute "set <F8>=\e[19;*~"
  execute "set <F9>=\e[20;*~"
  execute "set <F10>=\e[21;*~"
  execute "set <F11>=\e[23;*~"
  execute "set <F12>=\e[24;*~"

  execute "set t_kP=^[[5;*~"
  execute "set t_kN=^[[6;*~"
endfunction

if exists('$TMUX')
  call s:SetXtermCapabilities()
