#!/bin/sh

# Assignments and commands must use the first matching conditional branch.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
CONF=$(mktemp)
trap '$TMUX kill-server 2>/dev/null; rm -f "$CONF"' 0
trap 'exit 1' 1 2 15
$TMUX new-session -d || exit 1

branch()
{
	printf '%%%s %s%s' "$1" "$2" "$sep"
	printf '%sPARSE_BRANCH=%s%sset -g @branch %s%s' \
	    "$prefix" "$3" "$sep" "$3" "$sep"
}

# Cover no, one and two elif branches, with and without else, all condition
# values, and nesting inside both an active and an inactive outer branch.
for mode in normal hidden inline; do
	prefix=
	sep='
'
	[ "$mode" = hidden ] && prefix='%hidden '
	[ "$mode" = inline ] && sep=' '
	for elifs in 0 1 2; do
		bits=0
		while [ "$bits" -lt "$((1 << (elifs + 1)))" ]; do
			for otherwise in 0 1; do
				for outer in 0 1; do
					expected=unchanged
					{
						printf '%s\n' 'PARSE_BRANCH=unchanged' \
						    'set -g @branch unchanged'
						printf '%%if %s\n' "$outer"
						n=0
						while [ "$n" -le "$elifs" ]; do
							condition=$(((bits >> n) & 1))
							directive=elif
							[ "$n" -eq 0 ] && directive=if
							branch "$directive" "$condition" "$n"
							if [ "$condition" -eq 1 ] &&
							    [ "$expected" = unchanged ]; then
								expected=$n
							fi
							n=$((n + 1))
						done
						if [ "$otherwise" -eq 1 ]; then
							branch else '' else
							[ "$expected" = unchanged ] && expected=else
						fi
						printf '%s\n' '%endif' '%endif' \
						    'set -g @value "$PARSE_BRANCH"'
					} >"$CONF"
					[ "$outer" -eq 0 ] && expected=unchanged
					$TMUX source-file "$CONF" || exit 1
					out=$($TMUX display-message -p '#{@value}:#{@branch}')
					if [ "$out" != "$expected:$expected" ]; then
						echo "$mode, $elifs elifs, bits=$bits, else=$otherwise, outer=$outer"
						echo "Expected $expected:$expected, got $out"
						cat "$CONF"
						exit 1
					fi
				done
			done
			bits=$((bits + 1))
		done
	done
done

exit 0
