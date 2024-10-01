#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
sleep 1
$TMUX -f/dev/null new -x20 -y2 -d || exit 1
sleep 1
$TMUX set -g escape-time 0

exit_status=0

assert_key () {
	key=$1
	expected_code=$2

	W=$($TMUX new-window -P -- sh -c 'stty raw -echo && cat -tv')
	$TMUX send-keys -t$W "$key" 'EOL' || exit 1
	sleep 0.2

	actual_code=$($TMUX capturep -pt$W | \
			      head -1 | \
			      sed -e 's/EOL.*$//')
	$TMUX kill-window -t$W || exit 1

	if [ "$actual_code" = "$expected_code" ]; then
		if [ -n "$VERBOSE" ]; then
			echo "[PASS] $key -> $actual_code"
		fi
	else
		echo "[FAIL] $key -> $expected_code (Got: $actual_code)"
		exit_status=1
	fi

	shift
	shift

	if [ "$1" = "--" ]; then
		shift
		assert_key "$@"
	fi
}

assert_key 'C-Space' '^@'
assert_key 'C-a' '^A'	 -- 'M-C-a' '^[^A'
assert_key 'C-b' '^B'	 -- 'M-C-b' '^[^B'
assert_key 'C-c' '^C'	 -- 'M-C-c' '^[^C'
assert_key 'C-d' '^D'	 -- 'M-C-d' '^[^D'
assert_key 'C-e' '^E'	 -- 'M-C-e' '^[^E'
assert_key 'C-f' '^F'	 -- 'M-C-f' '^[^F'
assert_key 'C-g' '^G'	 -- 'M-C-g' '^[^G'
assert_key 'C-h' '^H'	 -- 'M-C-h' '^[^H'
assert_key 'C-i' '^I'	 -- 'M-C-i' '^[^I'
assert_key 'C-j' ''	 -- 'M-C-j' '^[' # NL
assert_key 'C-k' '^K'	 -- 'M-C-k' '^[^K'
assert_key 'C-l' '^L'	 -- 'M-C-l' '^[^L'
assert_key 'C-m' '^M'	 -- 'M-C-m' '^[^M'
assert_key 'C-n' '^N'	 -- 'M-C-n' '^[^N'
assert_key 'C-o' '^O'	 -- 'M-C-o' '^[^O'
assert_key 'C-p' '^P'	 -- 'M-C-p' '^[^P'
assert_key 'C-q' '^Q'	 -- 'M-C-q' '^[^Q'
assert_key 'C-r' '^R'	 -- 'M-C-r' '^[^R'
assert_key 'C-s' '^S'	 -- 'M-C-s' '^[^S'
assert_key 'C-t' '^T'	 -- 'M-C-t' '^[^T'
assert_key 'C-u' '^U'	 -- 'M-C-u' '^[^U'
assert_key 'C-v' '^V'	 -- 'M-C-v' '^[^V'
assert_key 'C-w' '^W'	 -- 'M-C-w' '^[^W'
assert_key 'C-x' '^X'	 -- 'M-C-x' '^[^X'
assert_key 'C-y' '^Y'	 -- 'M-C-y' '^[^Y'
assert_key 'C-z' '^Z'	 -- 'M-C-z' '^[^Z'
assert_key 'Escape' '^[' -- 'M-Escape' '^[^['
assert_key "C-\\" "^\\"	 -- "M-C-\\" "^[^\\"
assert_key 'C-]' '^]'	 -- 'M-C-]' '^[^]'
assert_key 'C-^' '^^'	 -- 'M-C-^' '^[^^'
assert_key 'C-_' '^_'	 -- 'M-C-_' '^[^_'
assert_key 'Space' ' '	 -- 'M-Space' '^[ '
assert_key '!' '!'	 -- 'M-!' '^[!'
assert_key '"' '"'	 -- 'M-"' '^["'
assert_key '#' '#'	 -- 'M-#' '^[#'
assert_key '$' '$'	 -- 'M-$' '^[$'
assert_key '%' '%'	 -- 'M-%' '^[%'
assert_key '&' '&'	 -- 'M-&' '^[&'
assert_key "'" "'"	 -- "M-'" "^['"
assert_key '(' '('	 -- 'M-(' '^[('
assert_key ')' ')'	 -- 'M-)' '^[)'
assert_key '*' '*'	 -- 'M-*' '^[*'
assert_key '+' '+'	 -- 'M-+' '^[+'
assert_key ',' ','	 -- 'M-,' '^[,'
assert_key '-' '-'	 -- 'M--' '^[-'
assert_key '.' '.'	 -- 'M-.' '^[.'
assert_key '/' '/'	 -- 'M-/' '^[/'
assert_key '0' '0'	 -- 'M-0' '^[0'
assert_key '1' '1'	 -- 'M-1' '^[1'
assert_key '2' '2'	 -- 'M-2' '^[2'
assert_key '3' '3'	 -- 'M-3' '^[3'
assert_key '4' '4'	 -- 'M-4' '^[4'
assert_key '5' '5'	 -- 'M-5' '^[5'
assert_key '6' '6'	 -- 'M-6' '^[6'
assert_key '7' '7'	 -- 'M-7' '^[7'
assert_key '8' '8'	 -- 'M-8' '^[8'
assert_key '9' '9'	 -- 'M-9' '^[9'
assert_key ':' ':'	 -- 'M-:' '^[:'
assert_key '\;' ';'	 -- 'M-\;' '^[;'
assert_key '<' '<'	 -- 'M-<' '^[<'
assert_key '=' '='	 -- 'M-=' '^[='
assert_key '>' '>'	 -- 'M->' '^[>'
assert_key '?' '?'	 -- 'M-?' '^[?'
assert_key '@' '@'	 -- 'M-@' '^[@'
assert_key 'A' 'A'	 -- 'M-A' '^[A'
assert_key 'B' 'B'	 -- 'M-B' '^[B'
assert_key 'C' 'C'	 -- 'M-C' '^[C'
assert_key 'D' 'D'	 -- 'M-D' '^[D'
assert_key 'E' 'E'	 -- 'M-E' '^[E'
assert_key 'F' 'F'	 -- 'M-F' '^[F'
assert_key 'G' 'G'	 -- 'M-G' '^[G'
assert_key 'H' 'H'	 -- 'M-H' '^[H'
assert_key 'I' 'I'	 -- 'M-I' '^[I'
assert_key 'J' 'J'	 -- 'M-J' '^[J'
assert_key 'K' 'K'	 -- 'M-K' '^[K'
assert_key 'L' 'L'	 -- 'M-L' '^[L'
assert_key 'M' 'M'	 -- 'M-M' '^[M'
assert_key 'N' 'N'	 -- 'M-N' '^[N'
assert_key 'O' 'O'	 -- 'M-O' '^[O'
assert_key 'P' 'P'	 -- 'M-P' '^[P'
assert_key 'Q' 'Q'	 -- 'M-Q' '^[Q'
assert_key 'R' 'R'	 -- 'M-R' '^[R'
assert_key 'S' 'S'	 -- 'M-S' '^[S'
assert_key 'T' 'T'	 -- 'M-T' '^[T'
assert_key 'U' 'U'	 -- 'M-U' '^[U'
assert_key 'V' 'V'	 -- 'M-V' '^[V'
assert_key 'W' 'W'	 -- 'M-W' '^[W'
assert_key 'X' 'X'	 -- 'M-X' '^[X'
assert_key 'Y' 'Y'	 -- 'M-Y' '^[Y'
assert_key 'Z' 'Z'	 -- 'M-Z' '^[Z'
assert_key '[' '['	 -- 'M-[' '^[['
assert_key "\\" "\\"	 -- "M-\\" "^[\\"
assert_key ']' ']'	 -- 'M-]' '^[]'
assert_key '^' '^'	 -- 'M-^' '^[^'
assert_key '_' '_'	 -- 'M-_' '^[_'
assert_key '`' '`'	 -- 'M-`' '^[`'
assert_key 'a' 'a'	 -- 'M-a' '^[a'
assert_key 'b' 'b'	 -- 'M-b' '^[b'
assert_key 'c' 'c'	 -- 'M-c' '^[c'
assert_key 'd' 'd'	 -- 'M-d' '^[d'
assert_key 'e' 'e'	 -- 'M-e' '^[e'
assert_key 'f' 'f'	 -- 'M-f' '^[f'
assert_key 'g' 'g'	 -- 'M-g' '^[g'
assert_key 'h' 'h'	 -- 'M-h' '^[h'
assert_key 'i' 'i'	 -- 'M-i' '^[i'
assert_key 'j' 'j'	 -- 'M-j' '^[j'
assert_key 'k' 'k'	 -- 'M-k' '^[k'
assert_key 'l' 'l'	 -- 'M-l' '^[l'
assert_key 'm' 'm'	 -- 'M-m' '^[m'
assert_key 'n' 'n'	 -- 'M-n' '^[n'
assert_key 'o' 'o'	 -- 'M-o' '^[o'
assert_key 'p' 'p'	 -- 'M-p' '^[p'
assert_key 'q' 'q'	 -- 'M-q' '^[q'
assert_key 'r' 'r'	 -- 'M-r' '^[r'
assert_key 's' 's'	 -- 'M-s' '^[s'
assert_key 't' 't'	 -- 'M-t' '^[t'
assert_key 'u' 'u'	 -- 'M-u' '^[u'
assert_key 'v' 'v'	 -- 'M-v' '^[v'
assert_key 'w' 'w'	 -- 'M-w' '^[w'
assert_key 'x' 'x'	 -- 'M-x' '^[x'
assert_key 'y' 'y'	 -- 'M-y' '^[y'
assert_key 'z' 'z'	 -- 'M-z' '^[z'
assert_key '{' '{'	 -- 'M-{' '^[{'
assert_key '|' '|'	 -- 'M-|' '^[|'
assert_key '}' '}'	 -- 'M-}' '^[}'
assert_key '~' '~'	 -- 'M-~' '^[~'

assert_key 'Tab' '^I'    -- 'M-Tab' '^[^I'
assert_key 'BSpace' '^?' -- 'M-BSpace' '^[^?'

## These cannot be sent, is that intentional?
## assert_key 'PasteStart' "^[[200~"
## assert_key 'PasteEnd' "^[[201~"

assert_key 'F1' "^[OP"
assert_key 'F2' "^[OQ"
assert_key 'F3' "^[OR"
assert_key 'F4' "^[OS"
assert_key 'F5' "^[[15~"
assert_key 'F6' "^[[17~"
assert_key 'F8' "^[[19~"
assert_key 'F9' "^[[20~"
assert_key 'F10' "^[[21~"
assert_key 'F11' "^[[23~"
assert_key 'F12' "^[[24~"

assert_key 'IC' '^[[2~'
assert_key 'Insert' '^[[2~'
assert_key 'DC' '^[[3~'
assert_key 'Delete' '^[[3~'

## Why do these differ from tty-keys?
assert_key 'Home' '^[[1~'
assert_key 'End' '^[[4~'

assert_key 'NPage' '^[[6~'
assert_key 'PageDown' '^[[6~'
assert_key 'PgDn' '^[[6~'
assert_key 'PPage' '^[[5~'
assert_key 'PageUp' '^[[5~'
assert_key 'PgUp' '^[[5~'

assert_key 'BTab' '^[[Z'
assert_key 'C-S-Tab' '^I'

assert_key 'Up' '^[[A'
assert_key 'Down' '^[[B'
assert_key 'Right' '^[[C'
assert_key 'Left' '^[[D'

# assert_key 'KPEnter'
assert_key 'KP*' '*' -- 'M-KP*' '^[*'
assert_key 'KP+' '+' -- 'M-KP+' '^[+'
assert_key 'KP-' '-' -- 'M-KP-' '^[-'
assert_key 'KP.' '.' -- 'M-KP.' '^[.'
assert_key 'KP/' '/' -- 'M-KP/' '^[/'
assert_key 'KP0' '0' -- 'M-KP0' '^[0'
assert_key 'KP1' '1' -- 'M-KP1' '^[1'
assert_key 'KP2' '2' -- 'M-KP2' '^[2'
assert_key 'KP3' '3' -- 'M-KP3' '^[3'
assert_key 'KP4' '4' -- 'M-KP4' '^[4'
assert_key 'KP5' '5' -- 'M-KP5' '^[5'
assert_key 'KP6' '6' -- 'M-KP6' '^[6'
assert_key 'KP7' '7' -- 'M-KP7' '^[7'
assert_key 'KP8' '8' -- 'M-KP8' '^[8'
assert_key 'KP9' '9' -- 'M-KP9' '^[9'

# Extended keys
$TMUX set -g extended-keys always

assert_extended_key () {
	extended_key=$1
	expected_code_pattern=$2

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;2/')
	assert_key "S-$extended_key" "$expected_code"

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;3/')
	assert_key "M-$extended_key" "$expected_code"

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;4/')
	assert_key "S-M-$extended_key" "$expected_code"

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;5/')
	assert_key "C-$extended_key" "$expected_code"

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;6/')
	assert_key "S-C-$extended_key" "$expected_code"

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;7/')
	assert_key "C-M-$extended_key" "$expected_code"

	expected_code=$(printf '%s' "$expected_code_pattern" | sed -e 's/;_/;8/')
	assert_key "S-C-M-$extended_key" "$expected_code"
}

## Many of these pass without extended keys enabled -- are they extended keys?
assert_extended_key 'F1' '^[[1;_P'
assert_extended_key 'F2' "^[[1;_Q"
assert_extended_key 'F3' "^[[1;_R"
assert_extended_key 'F4' "^[[1;_S"
assert_extended_key 'F5' "^[[15;_~"
assert_extended_key 'F6' "^[[17;_~"
assert_extended_key 'F8' "^[[19;_~"
assert_extended_key 'F9' "^[[20;_~"
assert_extended_key 'F10' "^[[21;_~"
assert_extended_key 'F11' "^[[23;_~"
assert_extended_key 'F12' "^[[24;_~"

assert_extended_key 'Up' '^[[1;_A'
assert_extended_key 'Down' '^[[1;_B'
assert_extended_key 'Right' '^[[1;_C'
assert_extended_key 'Left' '^[[1;_D'

assert_extended_key 'Home' '^[[1;_H'
assert_extended_key 'End' '^[[1;_F'

assert_extended_key 'PPage' '^[[5;_~'
assert_extended_key 'PageUp' '^[[5;_~'
assert_extended_key 'PgUp' '^[[5;_~'
assert_extended_key 'NPage' '^[[6;_~'
assert_extended_key 'PageDown' '^[[6;_~'
assert_extended_key 'PgDn' '^[[6;_~'

assert_extended_key 'IC' '^[[2;_~'
assert_extended_key 'Insert' '^[[2;_~'
assert_extended_key 'DC' '^[[3;_~'
assert_extended_key 'Delete' '^[[3;_~'

assert_key 'C-Tab' "^[[27;5;9~"
assert_key 'C-S-Tab' "^[[27;6;9~"

$TMUX kill-server 2>/dev/null

exit $exit_status
