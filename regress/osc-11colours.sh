#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d
$TMUX set -g remain-on-exit on

do_test() {
	$TMUX splitw "printf '$1'"
	sleep 0.25
	c="$($TMUX display -p '#{pane_bg}')"
	$TMUX kill-pane
	[ "$c" != "$2" ] && return 1
	return 0
}

do_test '\033]11;rgb:ff/ff/ff\007' '#ffffff' || exit 1
do_test '\033]11;rgb:ff/ff/ff\007\033]111\007' 'default' || exit 1

do_test '\033]11;cmy:0.9373/0.6941/0.4549\007' '#0f4e8b' || exit 1
do_test '\033]11;cmyk:0.88/0.44/0.00/0.45\007' '#104e8c' || exit 1

do_test '\033]11;16,78,139\007' '#104e8b' || exit 1
do_test '\033]11;#104E8B\007' '#104e8b' || exit 1
do_test '\033]11;#10004E008B00\007' '#104e8b' || exit 1
do_test '\033]11;DodgerBlue4\007' '#104e8b' || exit 1
do_test '\033]11;DodgerBlue4    \007' '#104e8b' || exit 1
do_test '\033]11;    DodgerBlue4\007' '#104e8b' || exit 1
do_test '\033]11;rgb:10/4E/8B\007' '#104e8b' || exit 1
do_test '\033]11;rgb:1000/4E00/8B00\007' '#104e8b' || exit 1

do_test '\033]11;grey\007' '#bebebe' || exit 1
do_test '\033]11;grey0\007' '#000000' || exit 1
do_test '\033]11;grey1\007' '#030303' || exit 1
do_test '\033]11;grey2\007' '#050505' || exit 1
do_test '\033]11;grey3\007' '#080808' || exit 1
do_test '\033]11;grey4\007' '#0a0a0a' || exit 1
do_test '\033]11;grey5\007' '#0d0d0d' || exit 1
do_test '\033]11;grey6\007' '#0f0f0f' || exit 1
do_test '\033]11;grey7\007' '#121212' || exit 1
do_test '\033]11;grey8\007' '#141414' || exit 1
do_test '\033]11;grey9\007' '#171717' || exit 1
do_test '\033]11;grey10\007' '#1a1a1a' || exit 1
do_test '\033]11;grey11\007' '#1c1c1c' || exit 1
do_test '\033]11;grey12\007' '#1f1f1f' || exit 1
do_test '\033]11;grey13\007' '#212121' || exit 1
do_test '\033]11;grey14\007' '#242424' || exit 1
do_test '\033]11;grey15\007' '#262626' || exit 1
do_test '\033]11;grey16\007' '#292929' || exit 1
do_test '\033]11;grey17\007' '#2b2b2b' || exit 1
do_test '\033]11;grey18\007' '#2e2e2e' || exit 1
do_test '\033]11;grey19\007' '#303030' || exit 1
do_test '\033]11;grey20\007' '#333333' || exit 1
do_test '\033]11;grey21\007' '#363636' || exit 1
do_test '\033]11;grey22\007' '#383838' || exit 1
do_test '\033]11;grey23\007' '#3b3b3b' || exit 1
do_test '\033]11;grey24\007' '#3d3d3d' || exit 1
do_test '\033]11;grey25\007' '#404040' || exit 1
do_test '\033]11;grey26\007' '#424242' || exit 1
do_test '\033]11;grey27\007' '#454545' || exit 1
do_test '\033]11;grey28\007' '#474747' || exit 1
do_test '\033]11;grey29\007' '#4a4a4a' || exit 1
do_test '\033]11;grey30\007' '#4d4d4d' || exit 1
do_test '\033]11;grey31\007' '#4f4f4f' || exit 1
do_test '\033]11;grey32\007' '#525252' || exit 1
do_test '\033]11;grey33\007' '#545454' || exit 1
do_test '\033]11;grey34\007' '#575757' || exit 1
do_test '\033]11;grey35\007' '#595959' || exit 1
do_test '\033]11;grey36\007' '#5c5c5c' || exit 1
do_test '\033]11;grey37\007' '#5e5e5e' || exit 1
do_test '\033]11;grey38\007' '#616161' || exit 1
do_test '\033]11;grey39\007' '#636363' || exit 1
do_test '\033]11;grey40\007' '#666666' || exit 1
do_test '\033]11;grey41\007' '#696969' || exit 1
do_test '\033]11;grey42\007' '#6b6b6b' || exit 1
do_test '\033]11;grey43\007' '#6e6e6e' || exit 1
do_test '\033]11;grey44\007' '#707070' || exit 1
do_test '\033]11;grey45\007' '#737373' || exit 1
do_test '\033]11;grey46\007' '#757575' || exit 1
do_test '\033]11;grey47\007' '#787878' || exit 1
do_test '\033]11;grey48\007' '#7a7a7a' || exit 1
do_test '\033]11;grey49\007' '#7d7d7d' || exit 1
do_test '\033]11;grey50\007' '#7f7f7f' || exit 1
do_test '\033]11;grey51\007' '#828282' || exit 1
do_test '\033]11;grey52\007' '#858585' || exit 1
do_test '\033]11;grey53\007' '#878787' || exit 1
do_test '\033]11;grey54\007' '#8a8a8a' || exit 1
do_test '\033]11;grey55\007' '#8c8c8c' || exit 1
do_test '\033]11;grey56\007' '#8f8f8f' || exit 1
do_test '\033]11;grey57\007' '#919191' || exit 1
do_test '\033]11;grey58\007' '#949494' || exit 1
do_test '\033]11;grey59\007' '#969696' || exit 1
do_test '\033]11;grey60\007' '#999999' || exit 1
do_test '\033]11;grey61\007' '#9c9c9c' || exit 1
do_test '\033]11;grey62\007' '#9e9e9e' || exit 1
do_test '\033]11;grey63\007' '#a1a1a1' || exit 1
do_test '\033]11;grey64\007' '#a3a3a3' || exit 1
do_test '\033]11;grey65\007' '#a6a6a6' || exit 1
do_test '\033]11;grey66\007' '#a8a8a8' || exit 1
do_test '\033]11;grey67\007' '#ababab' || exit 1
do_test '\033]11;grey68\007' '#adadad' || exit 1
do_test '\033]11;grey69\007' '#b0b0b0' || exit 1
do_test '\033]11;grey70\007' '#b3b3b3' || exit 1
do_test '\033]11;grey71\007' '#b5b5b5' || exit 1
do_test '\033]11;grey72\007' '#b8b8b8' || exit 1
do_test '\033]11;grey73\007' '#bababa' || exit 1
do_test '\033]11;grey74\007' '#bdbdbd' || exit 1
do_test '\033]11;grey75\007' '#bfbfbf' || exit 1
do_test '\033]11;grey76\007' '#c2c2c2' || exit 1
do_test '\033]11;grey77\007' '#c4c4c4' || exit 1
do_test '\033]11;grey78\007' '#c7c7c7' || exit 1
do_test '\033]11;grey79\007' '#c9c9c9' || exit 1
do_test '\033]11;grey80\007' '#cccccc' || exit 1
do_test '\033]11;grey81\007' '#cfcfcf' || exit 1
do_test '\033]11;grey82\007' '#d1d1d1' || exit 1
do_test '\033]11;grey83\007' '#d4d4d4' || exit 1
do_test '\033]11;grey84\007' '#d6d6d6' || exit 1
do_test '\033]11;grey85\007' '#d9d9d9' || exit 1
do_test '\033]11;grey86\007' '#dbdbdb' || exit 1
do_test '\033]11;grey87\007' '#dedede' || exit 1
do_test '\033]11;grey88\007' '#e0e0e0' || exit 1
do_test '\033]11;grey89\007' '#e3e3e3' || exit 1
do_test '\033]11;grey90\007' '#e5e5e5' || exit 1
do_test '\033]11;grey91\007' '#e8e8e8' || exit 1
do_test '\033]11;grey92\007' '#ebebeb' || exit 1
do_test '\033]11;grey93\007' '#ededed' || exit 1
do_test '\033]11;grey94\007' '#f0f0f0' || exit 1
do_test '\033]11;grey95\007' '#f2f2f2' || exit 1
do_test '\033]11;grey96\007' '#f5f5f5' || exit 1
do_test '\033]11;grey97\007' '#f7f7f7' || exit 1
do_test '\033]11;grey98\007' '#fafafa' || exit 1
do_test '\033]11;grey99\007' '#fcfcfc' || exit 1
do_test '\033]11;grey100\007' '#ffffff' || exit 1

do_test '\033]11;gray\007' '#bebebe' || exit 1
do_test '\033]11;gray0\007' '#000000' || exit 1
do_test '\033]11;gray1\007' '#030303' || exit 1
do_test '\033]11;gray2\007' '#050505' || exit 1
do_test '\033]11;gray3\007' '#080808' || exit 1
do_test '\033]11;gray4\007' '#0a0a0a' || exit 1
do_test '\033]11;gray5\007' '#0d0d0d' || exit 1
do_test '\033]11;gray6\007' '#0f0f0f' || exit 1
do_test '\033]11;gray7\007' '#121212' || exit 1
do_test '\033]11;gray8\007' '#141414' || exit 1
do_test '\033]11;gray9\007' '#171717' || exit 1
do_test '\033]11;gray10\007' '#1a1a1a' || exit 1
do_test '\033]11;gray11\007' '#1c1c1c' || exit 1
do_test '\033]11;gray12\007' '#1f1f1f' || exit 1
do_test '\033]11;gray13\007' '#212121' || exit 1
do_test '\033]11;gray14\007' '#242424' || exit 1
do_test '\033]11;gray15\007' '#262626' || exit 1
do_test '\033]11;gray16\007' '#292929' || exit 1
do_test '\033]11;gray17\007' '#2b2b2b' || exit 1
do_test '\033]11;gray18\007' '#2e2e2e' || exit 1
do_test '\033]11;gray19\007' '#303030' || exit 1
do_test '\033]11;gray20\007' '#333333' || exit 1
do_test '\033]11;gray21\007' '#363636' || exit 1
do_test '\033]11;gray22\007' '#383838' || exit 1
do_test '\033]11;gray23\007' '#3b3b3b' || exit 1
do_test '\033]11;gray24\007' '#3d3d3d' || exit 1
do_test '\033]11;gray25\007' '#404040' || exit 1
do_test '\033]11;gray26\007' '#424242' || exit 1
do_test '\033]11;gray27\007' '#454545' || exit 1
do_test '\033]11;gray28\007' '#474747' || exit 1
do_test '\033]11;gray29\007' '#4a4a4a' || exit 1
do_test '\033]11;gray30\007' '#4d4d4d' || exit 1
do_test '\033]11;gray31\007' '#4f4f4f' || exit 1
do_test '\033]11;gray32\007' '#525252' || exit 1
do_test '\033]11;gray33\007' '#545454' || exit 1
do_test '\033]11;gray34\007' '#575757' || exit 1
do_test '\033]11;gray35\007' '#595959' || exit 1
do_test '\033]11;gray36\007' '#5c5c5c' || exit 1
do_test '\033]11;gray37\007' '#5e5e5e' || exit 1
do_test '\033]11;gray38\007' '#616161' || exit 1
do_test '\033]11;gray39\007' '#636363' || exit 1
do_test '\033]11;gray40\007' '#666666' || exit 1
do_test '\033]11;gray41\007' '#696969' || exit 1
do_test '\033]11;gray42\007' '#6b6b6b' || exit 1
do_test '\033]11;gray43\007' '#6e6e6e' || exit 1
do_test '\033]11;gray44\007' '#707070' || exit 1
do_test '\033]11;gray45\007' '#737373' || exit 1
do_test '\033]11;gray46\007' '#757575' || exit 1
do_test '\033]11;gray47\007' '#787878' || exit 1
do_test '\033]11;gray48\007' '#7a7a7a' || exit 1
do_test '\033]11;gray49\007' '#7d7d7d' || exit 1
do_test '\033]11;gray50\007' '#7f7f7f' || exit 1
do_test '\033]11;gray51\007' '#828282' || exit 1
do_test '\033]11;gray52\007' '#858585' || exit 1
do_test '\033]11;gray53\007' '#878787' || exit 1
do_test '\033]11;gray54\007' '#8a8a8a' || exit 1
do_test '\033]11;gray55\007' '#8c8c8c' || exit 1
do_test '\033]11;gray56\007' '#8f8f8f' || exit 1
do_test '\033]11;gray57\007' '#919191' || exit 1
do_test '\033]11;gray58\007' '#949494' || exit 1
do_test '\033]11;gray59\007' '#969696' || exit 1
do_test '\033]11;gray60\007' '#999999' || exit 1
do_test '\033]11;gray61\007' '#9c9c9c' || exit 1
do_test '\033]11;gray62\007' '#9e9e9e' || exit 1
do_test '\033]11;gray63\007' '#a1a1a1' || exit 1
do_test '\033]11;gray64\007' '#a3a3a3' || exit 1
do_test '\033]11;gray65\007' '#a6a6a6' || exit 1
do_test '\033]11;gray66\007' '#a8a8a8' || exit 1
do_test '\033]11;gray67\007' '#ababab' || exit 1
do_test '\033]11;gray68\007' '#adadad' || exit 1
do_test '\033]11;gray69\007' '#b0b0b0' || exit 1
do_test '\033]11;gray70\007' '#b3b3b3' || exit 1
do_test '\033]11;gray71\007' '#b5b5b5' || exit 1
do_test '\033]11;gray72\007' '#b8b8b8' || exit 1
do_test '\033]11;gray73\007' '#bababa' || exit 1
do_test '\033]11;gray74\007' '#bdbdbd' || exit 1
do_test '\033]11;gray75\007' '#bfbfbf' || exit 1
do_test '\033]11;gray76\007' '#c2c2c2' || exit 1
do_test '\033]11;gray77\007' '#c4c4c4' || exit 1
do_test '\033]11;gray78\007' '#c7c7c7' || exit 1
do_test '\033]11;gray79\007' '#c9c9c9' || exit 1
do_test '\033]11;gray80\007' '#cccccc' || exit 1
do_test '\033]11;gray81\007' '#cfcfcf' || exit 1
do_test '\033]11;gray82\007' '#d1d1d1' || exit 1
do_test '\033]11;gray83\007' '#d4d4d4' || exit 1
do_test '\033]11;gray84\007' '#d6d6d6' || exit 1
do_test '\033]11;gray85\007' '#d9d9d9' || exit 1
do_test '\033]11;gray86\007' '#dbdbdb' || exit 1
do_test '\033]11;gray87\007' '#dedede' || exit 1
do_test '\033]11;gray88\007' '#e0e0e0' || exit 1
do_test '\033]11;gray89\007' '#e3e3e3' || exit 1
do_test '\033]11;gray90\007' '#e5e5e5' || exit 1
do_test '\033]11;gray91\007' '#e8e8e8' || exit 1
do_test '\033]11;gray92\007' '#ebebeb' || exit 1
do_test '\033]11;gray93\007' '#ededed' || exit 1
do_test '\033]11;gray94\007' '#f0f0f0' || exit 1
do_test '\033]11;gray95\007' '#f2f2f2' || exit 1
do_test '\033]11;gray96\007' '#f5f5f5' || exit 1
do_test '\033]11;gray97\007' '#f7f7f7' || exit 1
do_test '\033]11;gray98\007' '#fafafa' || exit 1
do_test '\033]11;gray99\007' '#fcfcfc' || exit 1
do_test '\033]11;gray100\007' '#ffffff' || exit 1

$TMUX -f/dev/null kill-server 2>/dev/null
exit 0
