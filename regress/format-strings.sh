#!/bin/sh

# Tests of formats as described in tmux(1) FORMATS

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"

# test_format $format $expected_result
test_format()
{
	fmt="$1"
	exp="$2"

	out=$($TMUX display-message -p "$fmt")

	if [ "$out" != "$exp" ]; then
		echo "Format test failed for '$fmt'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

# test_conditional_with_pane_in_mode $format $exp1 $exp2
#
# Tests the format string $format to yield $exp1 if #{pane_in_mode} is true and
# $exp2 when #{pane_in_mode} is false.
test_conditional_with_pane_in_mode()
{
	fmt="$1"
	exp_true="$2"
	exp_false="$3"

	$TMUX copy-mode # enter copy mode
	test_format "$fmt" "$exp_true"
	$TMUX send-keys -X cancel # leave copy mode
	test_format "$fmt" "$exp_false"
}

# test_conditional_with_session_name #format $exp_summer $exp_winter
#
# Tests the format string $format to yield $exp_summer if the session name is
# 'Summer' and $exp_winter if the session name is 'Winter'.
test_conditional_with_session_name()
{
	fmt="$1"
	exp_summer="$2"
	exp_winter="$3"

	$TMUX rename-session "Summer"
	test_format "$fmt" "$exp_summer"
	$TMUX rename-session "Winter"
	test_format "$fmt" "$exp_winter"
	$TMUX rename-session "Summer" # restore default
}


$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new-session -d || exit 1
$TMUX rename-session "Summer" || exit 1 # used later in conditionals

# Plain string without substitutions et al
test_format "abc xyz" "abc xyz"

# Test basic escapes for "#", "{", "#{" "}", "#}", ","
test_format "##" "#"
test_format "#," ","
test_format "{" "{"
test_format "##{" "#{"
test_format "#}" "}"
test_format "###}" "#}" # not a "basic" one but interesting nevertheless

# Simple expansion
test_format "#{pane_in_mode}" "0"

# Simple conditionals
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,xyz}" "abc" "xyz"

# Expansion in conditionals
test_conditional_with_pane_in_mode "#{?pane_in_mode,#{session_name},xyz}" "Summer" "xyz"

# Basic escapes in conditionals
# First argument
test_conditional_with_pane_in_mode "#{?pane_in_mode,##,xyz}" "#" "xyz"
test_conditional_with_pane_in_mode "#{?pane_in_mode,#,,xyz}" "," "xyz"
test_conditional_with_pane_in_mode "#{?pane_in_mode,{,xyz}" "{" "xyz"
test_conditional_with_pane_in_mode "#{?pane_in_mode,##{,xyz}" "#{" "xyz"
test_conditional_with_pane_in_mode "#{?pane_in_mode,#},xyz}" "}" "xyz"
# not a "basic" one but interesting nevertheless
test_conditional_with_pane_in_mode "#{?pane_in_mode,###},xyz}" "#}" "xyz"
# Second argument
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,##}" "abc" "#"
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,#,}" "abc" ","
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,{}" "abc" "{"
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,##{}" "abc" "#{"
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,#}}" "abc" "}"
# not a "basic" one but interesting nevertheless
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,###}}" "abc" "#}"
# mixed
test_conditional_with_pane_in_mode "#{?pane_in_mode,{,#}}" "{" "}"
test_conditional_with_pane_in_mode "#{?pane_in_mode,#},{}" "}" "{"
test_conditional_with_pane_in_mode "#{?pane_in_mode,##{,###}}" "#{" "#}"
test_conditional_with_pane_in_mode "#{?pane_in_mode,###},##{}" "#}" "#{"

# Conditionals split on the second comma (this is not documented)
test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,xyz,bonus}" "abc" "xyz,bonus"

# Curly brackets {...} do not capture a comma inside of conditionals as the
# conditional ends on the first '}'
test_conditional_with_pane_in_mode "#{?pane_in_mode,{abc,xyz},bonus}" "{abc,bonus}" "xyz,bonus}"

# Substitutions '#{...}' capture the comma
# invalid format: #{abc,xyz} is not a known variable name.
#test_conditional_with_pane_in_mode "#{?pane_in_mode,#{abc,xyz},bonus}" "" "bonus"

# Parenthesis (...) do not captura a comma
test_conditional_with_pane_in_mode "#{?pane_in_mode,(abc,xyz),bonus}" "(abc" "xyz),bonus"
test_conditional_with_pane_in_mode "#{?pane_in_mode,(abc#,xyz),bonus}" "(abc,xyz)" "bonus"

# Brackets [...] do not captura a comma
test_conditional_with_pane_in_mode "#{?pane_in_mode,[abc,xyz],bonus}" "[abc" "xyz],bonus"
test_conditional_with_pane_in_mode "#{?pane_in_mode,[abc#,xyz],bonus}" "[abc,xyz]" "bonus"


# Escape comma inside of #(...)
# Note: #() commands are run asynchronous and are substituted with result of the
# *previous* run or a placeholder (like "<'echo ,' not ready") if the command
# has not been run before. The format is updated as soon as the command
# finishes. As we are printing the message only once it never gets updated
# and the displayed message is "<'echo ,' not ready>"
test_format "#{?pane_in_mode,#(echo #,),xyz}" "xyz"
test_conditional_with_pane_in_mode "#{?pane_in_mode,#(echo #,),xyz}" "<'echo ,' not ready>" "xyz"
# This caching does not work :-(
#$TMUX display-message -p "#(echo #,)" > /dev/null
#test_conditional_with_pane_in_mode "#{?pane_in_mode,#(echo #,),xyz}" "," "xyz"
#test_conditional_with_pane_in_mode "#{?pane_in_mode,#(echo #,),xyz}" "," "xyz"

# invalid format: '#(' is not closed in the first argument of #{?,,}.
#test_conditional_with_pane_in_mode "#{?pane_in_mode,#(echo ,),xyz}" "" "),xyz"

# Escape comma inside of #[...]
test_conditional_with_pane_in_mode "#{?pane_in_mode,#[fg=default#,bg=default]abc,xyz}" "#[fg=default,bg=default]abc" "xyz"
# invalid format: '#[' is not closed in the first argument of #{?,,}
#test_conditional_with_pane_in_mode "#{?pane_in_mode,#[fg=default,bg=default]abc,xyz}" "" "bg=default]abc,xyz"

# Conditionals with comparison
test_conditional_with_session_name "#{?#{==:#{session_name},Summer},abc,xyz}" "abc" "xyz"
# Conditionals with comparison and escaped commas
$TMUX rename-session ","
test_format "#{?#{==:#,,#{session_name}},abc,xyz}" "abc"
$TMUX rename-session "Summer" # reset to default

# Conditional in conditional
test_conditional_with_pane_in_mode "#{?pane_in_mode,#{?#{==:#{session_name},Summer},ABC,XYZ},xyz}" "ABC" "xyz"
test_conditional_with_session_name "#{?pane_in_mode,#{?#{==:#{session_name},Summer},ABC,XYZ},xyz}" "xyz" "xyz"

test_conditional_with_pane_in_mode "#{?pane_in_mode,abc,#{?#{==:#{session_name},Summer},ABC,XYZ}}" "abc" "ABC"
test_conditional_with_session_name "#{?pane_in_mode,abc,#{?#{==:#{session_name},Summer},ABC,XYZ}}" "ABC" "XYZ"

# Some fancy stackings
test_conditional_with_pane_in_mode "#{?#{==:#{?pane_in_mode,#{session_name},#(echo Spring)},Summer},abc,xyz}" "abc" "xyz"



# Format test for the literal option
# Note: The behavior for #{l:...} with escapes is sometimes weird as #{l:...}
# respects the escapes.
test_format "#{l:#{}}" "#{}"
test_format "#{l:#{pane_in_mode}}" "#{pane_in_mode}"
test_format "#{l:#{?pane_in_mode,#{?#{==:#{session_name},Summer},ABC,XYZ},xyz}}" "#{?pane_in_mode,#{?#{==:#{session_name},Summer},ABC,XYZ},xyz}"

# With escapes (which escape but are returned literally)
test_format "#{l:##{}" "##{"
test_format "#{l:#{#}}}" "#{#}}"

# Invalid formats:
#test_format "#{l:#{}" ""
#test_format "#{l:#{#}}" ""

exit 0
