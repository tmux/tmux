#!/bin/sh

# Tests of fuzzy format matching.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

# test_format $format $expected_result
test_format()
{
	fmt="$1"
	exp="$2"

	out=$($TMUX display-message -p "$fmt")

	if [ "$out" != "$exp" ]; then
		echo "Fuzzy format test failed for '$fmt'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new-session -d || exit 1

# Empty pattern matches everything but has no highlighted positions.
test_format '#{m/z:,abc}' '1'
test_format '#{m/p:,abc}' ''

# Plain fuzzy matching and positions.
test_format '#{m/z:abc,a_b_c}' '1'
test_format '#{m/p:abc,a_b_c}' '0,2,4'
test_format '#{m/z:abc,acb}' '0'
test_format '#{m/p:abc,acb}' ''

# Multiple terms are ANDed. Positions are for all positive matched terms.
test_format '#{m/z:dev bash,dev:1 bash}' '1'
test_format '#{m/p:dev bash,dev:1 bash}' '0,1,2,6,7,8,9'
test_format '#{m/z:dev bash,dev:1 sh}' '0'
test_format '#{m/p:dev bash,dev:1 sh}' ''

# Lowercase patterns are case-insensitive for ASCII; uppercase makes the
# pattern case-sensitive.
test_format '#{m/z:abc,ABC}' '1'
test_format '#{m/p:abc,ABC}' '0,1,2'
test_format '#{m/z:ABC,abc}' '0'
test_format '#{m/p:ABC,abc}' ''

# Exact substring matching with a leading quote.
test_format "#{m/z:'bash,dev bash}" '1'
test_format "#{m/p:'bash,dev bash}" '4,5,6,7'
test_format "#{m/z:'bash,b-a-s-h}" '0'
test_format "#{m/p:'bash,b-a-s-h}" ''

# Prefix and suffix matching.
test_format '#{m/z:^dev,dev bash}' '1'
test_format '#{m/p:^dev,dev bash}' '0,1,2'
test_format '#{m/z:^dev,prod dev}' '0'
test_format '#{m/p:^dev,prod dev}' ''
test_format '#{m/z:bash$,dev bash}' '1'
test_format '#{m/p:bash$,dev bash}' '4,5,6,7'
test_format '#{m/z:bash$,bash dev}' '0'
test_format '#{m/p:bash$,bash dev}' ''

# Inverse terms. Plain inverse terms are exact substring tests, not fuzzy.
test_format '#{m/z:!ssh,dev bash}' '1'
test_format '#{m/z:!ssh,dev ssh}' '0'
test_format '#{m/z:!ssh,s_s_h}' '1'
test_format '#{m/z:dev !ssh,dev bash}' '1'
test_format '#{m/p:dev !ssh,dev bash}' '0,1,2'
test_format '#{m/z:dev !ssh,dev ssh}' '0'
test_format '#{m/p:dev !ssh,dev ssh}' ''

# OR separates groups; each group is an AND of its terms.
test_format '#{m/z:prod | dev,dev bash}' '1'
test_format '#{m/p:prod | dev,dev bash}' '0,1,2'
test_format '#{m/z:prod | dev,prod bash}' '1'
test_format '#{m/p:prod | dev,prod bash}' '0,1,2,3'
test_format '#{m/z:prod | dev,test bash}' '0'
test_format '#{m/p:prod | dev,test bash}' ''

# Style sequences do not count towards display positions.
test_format '#{m/z:dev,#[bold]dev#[default]}' '1'
test_format '#{m/p:dev,#[bold]dev#[default]}' '0,1,2'
test_format '#{m/p:dev,#[fg=red#,bg=blue]dev#[default]}' '0,1,2'
test_format '#{m/p:dev,#[bold]d#[default]e#[underscore]v#[default]}' '0,1,2'
test_format '#{m/p:bash,#[bold]dev#[default] bash}' '4,5,6,7'

# UTF-8. Non-ASCII matching is exact, even when ASCII would fold case.
test_format '#{m/z:é,café}' '1'
test_format '#{m/p:é,café}' '3'
test_format '#{m/z:é,É}' '0'
test_format '#{m/p:é,É}' ''
test_format '#{m/z:éx,éx}' '1'
test_format '#{m/p:éx,éx}' '0,1'

# Wide UTF-8 characters occupy two display cells and both should be marked for
# highlighting.
test_format '#{m/z:界,a界b}' '1'
test_format '#{m/p:界,a界b}' '1,2'

exit 0
