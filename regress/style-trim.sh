#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
TMUX2="$TEST_TMUX -Ltest2"
$TMUX2 kill-server 2>/dev/null

$TMUX2 -f/dev/null new -d "$TMUX -f/dev/null new"
sleep 2
$TMUX set -g status-style fg=default,bg=default

check() {
	v=$($TMUX display -p "$1")
	$TMUX set -g status-format[0] "$1"
	sleep 1
	r=$($TMUX2 capturep -Cep|tail -1|sed 's|\\033\[||g')

	if [ "$v" != "$2" -o "$r" != "$3" ]; then
		printf "$1 = [$v = $2] [$r = $3]"
		printf " \033[31mbad\033[0m\n"
		exit 1
	fi
}

# drawn as #0
$TMUX setenv -g V '#0'
check '#{V} #{w:V}' '#0 2' '#0 2'
check '#{=3:V}' '#0' '#0'
check '#{=-3:V}' '#0' '#0'

# drawn as #0
$TMUX setenv -g V '###[bg=yellow]0'
check '#{V} #{w:V}' '###[bg=yellow]0 2' '#43m0 249m'
check '#{=3:V}' '###[bg=yellow]0' '#43m049m'
check '#{=-3:V}' '###[bg=yellow]0' '#43m049m'

# drawn as #0123456
$TMUX setenv -g V '#0123456'
check '#{V} #{w:V}' '#0123456 8' '#0123456 8'
check '#{=3:V}' '#01' '#01'
check '#{=-3:V}' '456' '456'

# drawn as #0123456
$TMUX setenv -g V '##0123456'
check '#{V} #{w:V}' '##0123456 8' '#0123456 8'
check '#{=3:V}' '##01' '#01'
check '#{=-3:V}' '456' '456'

# drawn as ##0123456
$TMUX setenv -g V '###0123456'
check '#{V} #{w:V}' '###0123456 9' '##0123456 9'
check '#{=3:V}' '####0' '##0'
check '#{=-3:V}' '456' '456'

# drawn as 0123456
$TMUX setenv -g V '#[bg=yellow]0123456'
check '#{V} #{w:V}' '#[bg=yellow]0123456 7' '43m0123456 749m'
check '#{=3:V}' '#[bg=yellow]012' '43m01249m'
check '#{=-3:V}' '#[bg=yellow]456' '43m45649m'

# drawn as #[bg=yellow]0123456
$TMUX setenv -g V '##[bg=yellow]0123456'
check '#{V} #{w:V}' '##[bg=yellow]0123456 19' '#[bg=yellow]0123456 19'
check '#{=3:V}' '##[b' '#[b'
check '#{=-3:V}' '456' '456'

# drawn as #0123456
$TMUX setenv -g V '###[bg=yellow]0123456'
check '#{V} #{w:V}' '###[bg=yellow]0123456 8' '#43m0123456 849m'
check '#{=3:V}' '###[bg=yellow]01' '#43m0149m'
check '#{=-3:V}' '#[bg=yellow]456' '43m45649m'

# drawn as ##[bg=yellow]0123456
$TMUX setenv -g V '####[bg=yellow]0123456'
check '#{V} #{w:V}' '####[bg=yellow]0123456 20' '##[bg=yellow]0123456 20'
check '#{=3:V}' '####[' '##['
check '#{=-3:V}' '456' '456'

# drawn as ###0123456
$TMUX setenv -g V '#####[bg=yellow]0123456'
check '#{V} #{w:V}' '#####[bg=yellow]0123456 9' '##43m0123456 949m'
check '#{=3:V}' '#####[bg=yellow]0' '##43m049m'
check '#{=-3:V}' '#[bg=yellow]456' '43m45649m'

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
exit 0
