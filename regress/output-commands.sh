#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -L${TEST_SOCKET:-testO$$} -f/dev/null"
OUT=/tmp/tmux-output-commands-$$
EXPECTED=$(printf 'one\ntwo')

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -f "$OUT"
}
trap cleanup EXIT HUP INT TERM

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -x80 -y20 "sh -c 'printf \"\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\ntwo\\n\\033]133;D;0\\007separator\\n\\033]133;A\\007p\\$ \\033]133;B\\007broken\\n\\033]133;C\\007unfinished\"; exec sleep 100'" || exit 1
sleep 1

$TMUX copy-mode || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -X copy-output || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1

$TMUX send-keys -X pipe-output "wc -c >$OUT" || exit 1
sleep 1
[ "$(cat "$OUT")" = 10 ] || exit 1

$TMUX send-keys -X copy-pipe-output "wc -c >$OUT" output || exit 1
sleep 1
[ "$(cat "$OUT")" = 10 ] || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1

$TMUX send-keys -X select-output || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode || exit 1
$TMUX send-keys -X select-output || exit 1
$TMUX send-keys -X pipe-selection "wc -c >$OUT" || exit 1
sleep 1
[ "$(cat "$OUT")" = 10 ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode || exit 1
$TMUX send-keys -X search-backward separator || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -X copy-output || exit 1
[ "$($TMUX show-buffer)" = "$EXPECTED" ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode -c || exit 1
$TMUX send-keys -X search-backward echo || exit 1
$TMUX send-keys -X select-output || exit 1
$TMUX send-keys -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = "$EXPECTED" ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode || exit 1
$TMUX send-keys -X search-backward one || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -X copy-output || exit 1
[ "$($TMUX show-buffer)" = "$EXPECTED" ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX new-window -d -n plain "printf 'alpha\\nbeta\\n'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :plain || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :plain.0 -X copy-output -a || exit 1
all=$($TMUX show-buffer)
case "$all" in
*alpha*beta*) ;;
*) exit 1 ;;
esac
$TMUX send-keys -t :plain.0 -X cancel || exit 1

$TMUX set-buffer -b keep unchanged || exit 1
$TMUX copy-mode -t :plain || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :plain.0 -X copy-output || exit 1
[ "$($TMUX show-buffer -b keep)" = unchanged ] || exit 1
$TMUX send-keys -t :plain.0 -X cancel || exit 1

$TMUX new-window -d -n empty "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007true\\n\\033]133;C\\007\\033]133;D;0\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :empty || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :empty.0 -X copy-output || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1
$TMUX send-keys -t :empty.0 -X cancel || exit 1

$TMUX new-window -d -n prompt "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\033]133;C\\007one\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :prompt || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :prompt.0 -X copy-output || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1
$TMUX send-keys -t :prompt.0 -X cancel || exit 1

$TMUX copy-mode -c -t :prompt || exit 1
$TMUX send-keys -t :prompt.0 -X expand-output || exit 1
$TMUX send-keys -t :prompt.0 -X -N 100 cursor-down || exit 1
$TMUX send-keys -t :prompt.0 -X select-output || exit 1
[ "$($TMUX display-message -p -t :prompt.0 '#{selection_present}')" = 1 ] || exit 1
$TMUX send-keys -t :prompt.0 -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1
$TMUX send-keys -t :prompt.0 -X cancel || exit 1

$TMUX copy-mode -t :prompt || exit 1
$TMUX send-keys -t :prompt.0 -X -N 100 cursor-down || exit 1
$TMUX send-keys -t :prompt.0 C-o || exit 1
[ "$($TMUX display-message -p -t :prompt.0 '#{selection_present}')" = 1 ] || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :prompt.0 -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1
$TMUX send-keys -t :prompt.0 -X cancel || exit 1

$TMUX new-window -d -n same "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :same || exit 1
$TMUX send-keys -t :same.0 -X search-backward one || exit 1
$TMUX send-keys -t :same.0 -X select-output || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :same.0 -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1
$TMUX send-keys -t :same.0 -X cancel || exit 1

$TMUX set-option -g scroll-on-clear off || exit 1
$TMUX new-window -d -n clear "printf 'old1\\nold2\\nold3\\nold4\\nold5\\nold6\\n\\033]133;A\\007p\\$ \\033]133;B\\007echo 1; clear; ps\\n\\033]133;C\\0071\\n\\033[H\\033[2JPID TTY\\n1 pts/0\\n\\033]133;D;0\\007separator\\n\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :clear || exit 1
$TMUX send-keys -t :clear.0 C-o || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :clear.0 -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = "$(printf 'PID TTY\n1 pts/0')" ] || exit 1
$TMUX send-keys -t :clear.0 -X cancel || exit 1

# Select from either output line after clearing the screen.
for view in normal unfolded; do
	for steps in 2 3; do
		if [ "$view" = unfolded ]; then
			$TMUX copy-mode -U -t :clear || exit 1
		else
			$TMUX copy-mode -t :clear || exit 1
		fi
		$TMUX send-keys -t :clear.0 -X -N "$steps" cursor-up || exit 1
		$TMUX send-keys -t :clear.0 C-o || exit 1
		[ "$($TMUX display-message -p -t :clear.0 '#{selection_present}')" = 1 ] || exit 1
		$TMUX set-buffer sentinel || exit 1
		$TMUX send-keys -t :clear.0 -X copy-selection || exit 1
		[ "$($TMUX show-buffer)" = "$(printf 'PID TTY\n1 pts/0')" ] || exit 1
		$TMUX send-keys -t :clear.0 -X cancel || exit 1
	done
done

$TMUX set-option -g scroll-on-clear on || exit 1
$TMUX new-window -d -n clearprompt "printf '\\033]133;A\\007p\\$ \\033]133;B\\007clear; echo hello; echo world\\n\\033]133;C\\007\\033[H\\033[2J\\033[3Jhello\\nworld\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
for steps in 1 2; do
	$TMUX copy-mode -t :clearprompt || exit 1
	$TMUX send-keys -t :clearprompt.0 -X -N "$steps" cursor-up || exit 1
	$TMUX send-keys -t :clearprompt.0 C-o || exit 1
	[ "$($TMUX display-message -p -t :clearprompt.0 '#{selection_present}')" = 1 ] || exit 1
	$TMUX set-buffer sentinel || exit 1
	$TMUX send-keys -t :clearprompt.0 -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = "$(printf 'hello\nworld')" ] || exit 1
	$TMUX send-keys -t :clearprompt.0 -X cancel || exit 1
done

# A screen clear must not pull earlier commands or plain scrollback into output.
for integration in plain marked; do
	for erase in J 2J; do
		$TMUX set-option -g scroll-on-clear off || exit 1
		if [ "$integration" = marked ]; then
			prefix='\033]133;A\007old>\033]133;B\007seq\n\033]133;C\007'
			ending='\033]133;D;0\007'
		else
			prefix=
			ending=
		fi
		$TMUX new-window -d -n retained "printf '$prefix'; seq 1 40; printf '$ending\\033]133;A\\007p\\$ \\033]133;B\\007clear; echo hello; echo world\\n\\033]133;C\\007\\033[H\\033[${erase}hello\\nworld\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
		sleep 1
		[ "$($TMUX display-message -p -t :retained.0 '#{history_size}')" -gt 0 ] || exit 1
		for steps in 0 1 2; do
			$TMUX copy-mode -t :retained || exit 1
			if [ "$steps" -gt 0 ]; then
				$TMUX send-keys -t :retained.0 -X -N "$steps" cursor-up || exit 1
			fi
			$TMUX send-keys -t :retained.0 C-o || exit 1
			[ "$($TMUX display-message -p -t :retained.0 '#{selection_present}')" = 1 ] || exit 1
			$TMUX set-buffer sentinel || exit 1
			$TMUX send-keys -t :retained.0 -X copy-selection || exit 1
			[ "$($TMUX show-buffer)" = "$(printf 'hello\nworld')" ] || exit 1
			$TMUX send-keys -t :retained.0 -X cancel || exit 1
		done
		$TMUX kill-window -t :retained || exit 1
	done
done

# Preserve C when scroll-on-clear keeps the command's earlier output.
$TMUX set-option -g scroll-on-clear on || exit 1
$TMUX new-window -d -n preserve "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo before; clear; echo hello; echo world\\n\\033]133;C\\007before\\n\\033[H\\033[2Jhello\\nworld\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :preserve || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :preserve.0 -X copy-output || exit 1
[ "$($TMUX show-buffer)" = "$(printf 'before\nhello\nworld')" ] || exit 1
$TMUX send-keys -t :preserve.0 -X cancel || exit 1

# An unfinished command can be selected using its restored C without D.
$TMUX set-option -g scroll-on-clear off || exit 1
$TMUX new-window -d -n running "seq 1 40; printf '\\033]133;A\\007p\\$ \\033]133;B\\007clear; echo hello; echo world\\n\\033]133;C\\007\\033[H\\033[2Jhello\\nworld\\n'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :running || exit 1
$TMUX send-keys -t :running.0 -X cursor-up || exit 1
$TMUX send-keys -t :running.0 C-o || exit 1
[ "$($TMUX display-message -p -t :running.0 '#{selection_present}')" = 1 ] || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :running.0 -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = "$(printf 'hello\nworld')" ] || exit 1
$TMUX send-keys -t :running.0 -X cancel || exit 1

# Do not infer output from a D which belongs to the first surviving prompt.
$TMUX new-window -d -n prefix "printf 'prefix\\n\\033]133;A\\007p\\$ \\033]133;B\\007echo\\033]133;C\\007one\\033]133;D;0\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :prefix || exit 1
$TMUX send-keys -t :prefix.0 -X cursor-up || exit 1
$TMUX send-keys -t :prefix.0 C-o || exit 1
[ "$($TMUX display-message -p -t :prefix.0 '#{selection_present}')" = 0 ] || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :prefix.0 -X copy-output || exit 1
[ "$($TMUX show-buffer)" = sentinel ] || exit 1
$TMUX send-keys -t :prefix.0 -X cancel || exit 1

$TMUX new-window -d -n oneline "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\033]133;D;0\\007\\nseparator\\n\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :oneline || exit 1
$TMUX send-keys -t :oneline.0 -X search-backward echo || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :oneline.0 -X copy-output || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1
$TMUX send-keys -t :oneline.0 -X cancel || exit 1

$TMUX new-window -d -n nextprompt "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\ntwo\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-mode -t :nextprompt || exit 1
$TMUX send-keys -t :nextprompt.0 -X search-backward echo || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :nextprompt.0 -X copy-output || exit 1
[ "$($TMUX show-buffer)" = "$EXPECTED" ] || exit 1
$TMUX send-keys -t :nextprompt.0 -X cancel || exit 1

$TMUX new-window -d -n hist "printf '\\033]133;A\\007p\\$ \\033]133;B\\007seq\\n\\033]133;C\\007'; seq 1 40; sleep 3; seq 100 103; printf '\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX clear-history -t :hist.0 || exit 1
sleep 4
$TMUX copy-mode -t :hist || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t :hist.0 -X copy-output || exit 1
$TMUX show-buffer >$OUT
[ "$(head -n1 "$OUT")" != sentinel ] || exit 1
[ "$(head -n1 "$OUT")" != 1 ] || exit 1
[ "$(tail -n1 "$OUT")" = 103 ] || exit 1
grep -q seq "$OUT" && exit 1
$TMUX send-keys -t :hist.0 -X cancel || exit 1

exit 0
