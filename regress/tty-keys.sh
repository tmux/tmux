#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
TMUX2="$TEST_TMUX -Ltest2"
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX2 -f/dev/null new -d || exit 1
$TMUX -f/dev/null new -d "$TMUX2 attach" || exit 1
sleep 1

exit_status=0

format_string () {
	case $1 in
		*\')
			printf '"%%%%"'
			;;
		*)
			printf "'%%%%'"
			;;
	esac
}

assert_key () {
	keys=$1
	expected_name=$2
	format_string=$(format_string "$expected_name")

	$TMUX2 command-prompt -k 'display-message -pl '"$format_string" > "$TMP" &
	sleep 0.05

	$TMUX send-keys $keys

	wait

	keys=$(printf '%s' "$keys" | sed -e 's/Escape/\\\\033/g' | tr -d '[:space:]')
	actual_name=$(tr -d '[:space:]' < "$TMP")

	if [ "$actual_name" = "$expected_name" ]; then
		if [ -n "$VERBOSE" ]; then
			echo "[PASS] $keys -> $actual_name"
		fi
	else
		echo "[FAIL] $keys -> $expected_name (Got: '$actual_name')"
		exit_status=1
	fi

	if [ "$3" = "--" ]; then
		shift; shift; shift
		assert_key "$@"
	fi

}

assert_key 0x00 'C-Space' # -- 'Escape 0x00' 'C-M-Space'
assert_key 0x01 'C-a'	  -- 'Escape 0x01' 'C-M-a'
assert_key 0x02 'C-b'	  -- 'Escape 0x02' 'C-M-b'
assert_key 0x03 'C-c'	  -- 'Escape 0x03' 'C-M-c'
assert_key 0x04 'C-d'	  -- 'Escape 0x04' 'C-M-d'
assert_key 0x05 'C-e'	  -- 'Escape 0x05' 'C-M-e'
assert_key 0x06 'C-f'	  -- 'Escape 0x06' 'C-M-f'
assert_key 0x07 'C-g'	  -- 'Escape 0x07' 'C-M-g'
assert_key 0x08 'C-h'	  -- 'Escape 0x08' 'C-M-h'
assert_key 0x09 'Tab'	  -- 'Escape 0x09' 'M-Tab'
assert_key 0x0A 'C-j'	  -- 'Escape 0x0A' 'C-M-j'
assert_key 0x0B 'C-k'	  -- 'Escape 0x0B' 'C-M-k'
assert_key 0x0C 'C-l'	  -- 'Escape 0x0C' 'C-M-l'
assert_key 0x0D 'Enter'	  -- 'Escape 0x0D' 'M-Enter'
assert_key 0x0E 'C-n'	  -- 'Escape 0x0E' 'C-M-n'
assert_key 0x0F 'C-o'	  -- 'Escape 0x0F' 'C-M-o'
assert_key 0x10 'C-p'	  -- 'Escape 0x10' 'C-M-p'
assert_key 0x11 'C-q'	  -- 'Escape 0x11' 'C-M-q'
assert_key 0x12 'C-r'	  -- 'Escape 0x12' 'C-M-r'
assert_key 0x13 'C-s'	  -- 'Escape 0x13' 'C-M-s'
assert_key 0x14 'C-t'	  -- 'Escape 0x14' 'C-M-t'
assert_key 0x15 'C-u'	  -- 'Escape 0x15' 'C-M-u'
assert_key 0x16 'C-v'	  -- 'Escape 0x16' 'C-M-v'
assert_key 0x17 'C-w'	  -- 'Escape 0x17' 'C-M-w'
assert_key 0x18 'C-x'	  -- 'Escape 0x18' 'C-M-x'
assert_key 0x19 'C-y'	  -- 'Escape 0x19' 'C-M-y'
assert_key 0x1A 'C-z'	  -- 'Escape 0x1A' 'C-M-z'
assert_key 0x1B 'Escape'  -- 'Escape 0x1B' 'M-Escape'
assert_key 0x1C "C-\\"	  -- 'Escape 0x1C' "C-M-\\"
assert_key 0x1D 'C-]'	  -- 'Escape 0x1D' 'C-M-]'
assert_key 0x1E 'C-^'	  -- 'Escape 0x1E' 'C-M-^'
assert_key 0x1F 'C-_'	  -- 'Escape 0x1F' 'C-M-_'
assert_key 0x20 'Space'	  -- 'Escape 0x20' 'M-Space'
assert_key 0x21 '!'	  -- 'Escape 0x21' 'M-!'
assert_key 0x22 '"'	  -- 'Escape 0x22' 'M-"'
assert_key 0x23 '#'	  -- 'Escape 0x23'= 'M-#'
assert_key 0x24 '$'	  -- 'Escape 0x24'= 'M-$'
assert_key 0x25 '%'	  -- 'Escape 0x25'= 'M-%'
assert_key 0x26 '&'	  -- 'Escape 0x26'= 'M-&'
assert_key 0x27 "'"	  -- 'Escape 0x27' "M-'"
assert_key 0x28 '('	  -- 'Escape 0x28' 'M-('
assert_key 0x29 ')'	  -- 'Escape 0x29' 'M-)'
assert_key 0x2A '*'	  -- 'Escape 0x2A' 'M-*'
assert_key 0x2B '+'	  -- 'Escape 0x2B' 'M-+'
assert_key 0x2C ','	  -- 'Escape 0x2C' 'M-,'
assert_key 0x2D '-'	  -- 'Escape 0x2D' 'M--'
assert_key 0x2E '.'	  -- 'Escape 0x2E' 'M-.'
assert_key 0x2F '/'	  -- 'Escape 0x2F' 'M-/'
assert_key 0x30 '0'	  -- 'Escape 0x30' 'M-0'
assert_key 0x31 '1'	  -- 'Escape 0x31' 'M-1'
assert_key 0x32 '2'	  -- 'Escape 0x32' 'M-2'
assert_key 0x33 '3'	  -- 'Escape 0x33' 'M-3'
assert_key 0x34 '4'	  -- 'Escape 0x34' 'M-4'
assert_key 0x35 '5'	  -- 'Escape 0x35' 'M-5'
assert_key 0x36 '6'	  -- 'Escape 0x36' 'M-6'
assert_key 0x37 '7'	  -- 'Escape 0x37' 'M-7'
assert_key 0x38 '8'	  -- 'Escape 0x38' 'M-8'
assert_key 0x39 '9'	  -- 'Escape 0x39' 'M-9'
assert_key 0x3A ':'	  -- 'Escape 0x3A' 'M-:'
assert_key 0x3B ';'	  -- 'Escape 0x3B' 'M-;'
assert_key 0x3C '<'	  -- 'Escape 0x3C' 'M-<'
assert_key 0x3D '='	  -- 'Escape 0x3D' 'M-='
assert_key 0x3E '>'	  -- 'Escape 0x3E' 'M->'
assert_key 0x3F '?'	  -- 'Escape 0x3F' 'M-?'
assert_key 0x40 '@'	  -- 'Escape 0x40' 'M-@'
assert_key 0x41 'A'	  -- 'Escape 0x41' 'M-A'
assert_key 0x42 'B'	  -- 'Escape 0x42' 'M-B'
assert_key 0x43 'C'	  -- 'Escape 0x43' 'M-C'
assert_key 0x44 'D'	  -- 'Escape 0x44' 'M-D'
assert_key 0x45 'E'	  -- 'Escape 0x45' 'M-E'
assert_key 0x46 'F'	  -- 'Escape 0x46' 'M-F'
assert_key 0x47 'G'	  -- 'Escape 0x47' 'M-G'
assert_key 0x48 'H'	  -- 'Escape 0x48' 'M-H'
assert_key 0x49 'I'	  -- 'Escape 0x49' 'M-I'
assert_key 0x4A 'J'	  -- 'Escape 0x4A' 'M-J'
assert_key 0x4B 'K'	  -- 'Escape 0x4B' 'M-K'
assert_key 0x4C 'L'	  -- 'Escape 0x4C' 'M-L'
assert_key 0x4D 'M'	  -- 'Escape 0x4D' 'M-M'
assert_key 0x4E 'N'	  -- 'Escape 0x4E' 'M-N'
assert_key 0x4F 'O'	  -- 'Escape 0x4F' 'M-O'
assert_key 0x50 'P'	  -- 'Escape 0x50' 'M-P'
assert_key 0x51 'Q'	  -- 'Escape 0x51' 'M-Q'
assert_key 0x52 'R'	  -- 'Escape 0x52' 'M-R'
assert_key 0x53 'S'	  -- 'Escape 0x53' 'M-S'
assert_key 0x54 'T'	  -- 'Escape 0x54' 'M-T'
assert_key 0x55 'U'	  -- 'Escape 0x55' 'M-U'
assert_key 0x56 'V'	  -- 'Escape 0x56' 'M-V'
assert_key 0x57 'W'	  -- 'Escape 0x57' 'M-W'
assert_key 0x58 'X'	  -- 'Escape 0x58' 'M-X'
assert_key 0x59 'Y'	  -- 'Escape 0x59' 'M-Y'
assert_key 0x5A 'Z'	  -- 'Escape 0x5A' 'M-Z'
assert_key 0x5B '['	  -- 'Escape 0x5B' 'M-['
assert_key 0x5C "\\"	  -- 'Escape 0x5C' "M-\\"
assert_key 0x5D ']'	  -- 'Escape 0x5D' 'M-]'
assert_key 0x5E '^'	  -- 'Escape 0x5E' 'M-^'
assert_key 0x5F '_'	  -- 'Escape 0x5F' 'M-_'
assert_key 0x60 '`'	  -- 'Escape 0x60' 'M-`'
assert_key 0x61 'a'	  -- 'Escape 0x61' 'M-a'
assert_key 0x62 'b'	  -- 'Escape 0x62' 'M-b'
assert_key 0x63 'c'	  -- 'Escape 0x63' 'M-c'
assert_key 0x64 'd'	  -- 'Escape 0x64' 'M-d'
assert_key 0x65 'e'	  -- 'Escape 0x65' 'M-e'
assert_key 0x66 'f'	  -- 'Escape 0x66' 'M-f'
assert_key 0x67 'g'	  -- 'Escape 0x67' 'M-g'
assert_key 0x68 'h'	  -- 'Escape 0x68' 'M-h'
assert_key 0x69 'i'	  -- 'Escape 0x69' 'M-i'
assert_key 0x6A 'j'	  -- 'Escape 0x6A' 'M-j'
assert_key 0x6B 'k'	  -- 'Escape 0x6B' 'M-k'
assert_key 0x6C 'l'	  -- 'Escape 0x6C' 'M-l'
assert_key 0x6D 'm'	  -- 'Escape 0x6D' 'M-m'
assert_key 0x6E 'n'	  -- 'Escape 0x6E' 'M-n'
assert_key 0x6F 'o'	  -- 'Escape 0x6F' 'M-o'
assert_key 0x70 'p'	  -- 'Escape 0x70' 'M-p'
assert_key 0x71 'q'	  -- 'Escape 0x71' 'M-q'
assert_key 0x72 'r'	  -- 'Escape 0x72' 'M-r'
assert_key 0x73 's'	  -- 'Escape 0x73' 'M-s'
assert_key 0x74 't'	  -- 'Escape 0x74' 'M-t'
assert_key 0x75 'u'	  -- 'Escape 0x75' 'M-u'
assert_key 0x76 'v'	  -- 'Escape 0x76' 'M-v'
assert_key 0x77 'w'	  -- 'Escape 0x77' 'M-w'
assert_key 0x78 'x'	  -- 'Escape 0x78' 'M-x'
assert_key 0x79 'y'	  -- 'Escape 0x79' 'M-y'
assert_key 0x7A 'z'	  -- 'Escape 0x7A' 'M-z'
assert_key 0x7B '{'	  -- 'Escape 0x7B' 'M-{'
assert_key 0x7C '|'	  -- 'Escape 0x7C' 'M-|'
assert_key 0x7D '}'	  -- 'Escape 0x7D' 'M-}'
assert_key 0x7E '~'	  -- 'Escape 0x7E' 'M-~'
assert_key 0x7F 'BSpace'  -- 'Escape 0x7F' 'M-BSpace'

# Numeric keypad
assert_key 'Escape OM' 'KPEnter' -- 'Escape Escape OM' 'M-KPEnter'
assert_key 'Escape Oj' 'KP*'	 -- 'Escape Escape Oj' 'M-KP*'
assert_key 'Escape Ok' 'KP+'	 -- 'Escape Escape Ok' 'M-KP+'
assert_key 'Escape Om' 'KP-'	 -- 'Escape Escape Om' 'M-KP-'
assert_key 'Escape On' 'KP.'	 -- 'Escape Escape On' 'M-KP.'
assert_key 'Escape Oo' 'KP/'	 -- 'Escape Escape Oo' 'M-KP/'
assert_key 'Escape Op' 'KP0'	 -- 'Escape Escape Op' 'M-KP0'
assert_key 'Escape Oq' 'KP1'	 -- 'Escape Escape Oq' 'M-KP1'
assert_key 'Escape Or' 'KP2'	 -- 'Escape Escape Or' 'M-KP2'
assert_key 'Escape Os' 'KP3'	 -- 'Escape Escape Os' 'M-KP3'
assert_key 'Escape Ot' 'KP4'	 -- 'Escape Escape Ot' 'M-KP4'
assert_key 'Escape Ou' 'KP5'	 -- 'Escape Escape Ou' 'M-KP5'
assert_key 'Escape Ov' 'KP6'	 -- 'Escape Escape Ov' 'M-KP6'
assert_key 'Escape Ow' 'KP7'	 -- 'Escape Escape Ow' 'M-KP7'
assert_key 'Escape Ox' 'KP8'	 -- 'Escape Escape Ox' 'M-KP8'
assert_key 'Escape Oy' 'KP9'	 -- 'Escape Escape Oy' 'M-KP9'

# Arrow keys
assert_key 'Escape OA' 'Up'    -- 'Escape Escape OA' 'M-Up'
assert_key 'Escape OB' 'Down'  -- 'Escape Escape OB' 'M-Down'
assert_key 'Escape OC' 'Right' -- 'Escape Escape OC' 'M-Right'
assert_key 'Escape OD' 'Left'  -- 'Escape Escape OD' 'M-Left'

assert_key 'Escape [A' 'Up'    -- 'Escape Escape [A' 'M-Up'
assert_key 'Escape [B' 'Down'  -- 'Escape Escape [B' 'M-Down'
assert_key 'Escape [C' 'Right' -- 'Escape Escape [C' 'M-Right'
assert_key 'Escape [D' 'Left'  -- 'Escape Escape [D' 'M-Left'

# Other xterm keys
assert_key 'Escape OH' 'Home' -- 'Escape Escape OH' 'M-Home'
assert_key 'Escape OF' 'End'  -- 'Escape Escape OF' 'M-End'

assert_key 'Escape [H' 'Home' -- 'Escape Escape [H' 'M-Home'
assert_key 'Escape [F' 'End'  -- 'Escape Escape [F' 'M-End'

# rxvt arrow keys
assert_key 'Escape Oa' 'C-Up'
assert_key 'Escape Ob' 'C-Down'
assert_key 'Escape Oc' 'C-Right'
assert_key 'Escape Od' 'C-Left'
assert_key 'Escape [a' 'S-Up'
assert_key 'Escape [b' 'S-Down'
assert_key 'Escape [c' 'S-Right'
assert_key 'Escape [d' 'S-Left'

# rxvt function keys
assert_key 'Escape [11~' 'F1'
assert_key 'Escape [12~' 'F2'
assert_key 'Escape [13~' 'F3'
assert_key 'Escape [14~' 'F4'
assert_key 'Escape [15~' 'F5'
assert_key 'Escape [17~' 'F6'
assert_key 'Escape [18~' 'F7'
assert_key 'Escape [19~' 'F8'
assert_key 'Escape [20~' 'F9'
assert_key 'Escape [21~' 'F10'
assert_key 'Escape [23~' 'F11'
assert_key 'Escape [24~' 'F12'

# With TERM=screen, these will be seen as F11 and F12
# assert_key 'Escape [23~' 'S-F1'
# assert_key 'Escape [24~' 'S-F2'
assert_key 'Escape [25~' 'S-F3'
assert_key 'Escape [26~' 'S-F4'
assert_key 'Escape [28~' 'S-F5'
assert_key 'Escape [29~' 'S-F6'
assert_key 'Escape [31~' 'S-F7'
assert_key 'Escape [32~' 'S-F8'
assert_key 'Escape [33~' 'S-F9'
assert_key 'Escape [34~' 'S-F10'
assert_key 'Escape [23$' 'S-F11'
assert_key 'Escape [24$' 'S-F12'

assert_key 'Escape [11^' 'C-F1'
assert_key 'Escape [12^' 'C-F2'
assert_key 'Escape [13^' 'C-F3'
assert_key 'Escape [14^' 'C-F4'
assert_key 'Escape [15^' 'C-F5'
assert_key 'Escape [17^' 'C-F6'
assert_key 'Escape [18^' 'C-F7'
assert_key 'Escape [19^' 'C-F8'
assert_key 'Escape [20^' 'C-F9'
assert_key 'Escape [21^' 'C-F10'
assert_key 'Escape [23^' 'C-F11'
assert_key 'Escape [24^' 'C-F12'

assert_key 'Escape [11@' 'C-S-F1'
assert_key 'Escape [12@' 'C-S-F2'
assert_key 'Escape [13@' 'C-S-F3'
assert_key 'Escape [14@' 'C-S-F4'
assert_key 'Escape [15@' 'C-S-F5'
assert_key 'Escape [17@' 'C-S-F6'
assert_key 'Escape [18@' 'C-S-F7'
assert_key 'Escape [19@' 'C-S-F8'
assert_key 'Escape [20@' 'C-S-F9'
assert_key 'Escape [21@' 'C-S-F10'
assert_key 'Escape [23@' 'C-S-F11'
assert_key 'Escape [24@' 'C-S-F12'

# Focus tracking
assert_key 'Escape [I' 'FocusIn'
assert_key 'Escape [O' 'FocusOut'

# Paste keys
assert_key 'Escape [200~' 'PasteStart'
assert_key 'Escape [201~' 'PasteEnd'

assert_key 'Escape [Z' 'BTab'

assert_extended_key () {
	code=$1
	key_name=$2

	assert_key "Escape [${code};5u" "C-$key_name"
	assert_key "Escape [${code};7u" "C-M-$key_name"
}

# Extended keys
# assert_extended_key 65 'A'
# assert_extended_key 66 'B'
# assert_extended_key 67 'C'
# assert_extended_key 68 'D'
# assert_extended_key 69 'E'
# assert_extended_key 70 'F'
# assert_extended_key 71 'G'
# assert_extended_key 72 'H'
# assert_extended_key 73 'I'
# assert_extended_key 74 'J'
# assert_extended_key 75 'K'
# assert_extended_key 76 'L'
# assert_extended_key 77 'M'
# assert_extended_key 78 'N'
# assert_extended_key 79 'O'
# assert_extended_key 80 'P'
# assert_extended_key 81 'Q'
# assert_extended_key 82 'R'
# assert_extended_key 83 'S'
# assert_extended_key 84 'T'
# assert_extended_key 85 'U'
# assert_extended_key 86 'V'
# assert_extended_key 87 'W'
# assert_extended_key 88 'X'
# assert_extended_key 89 'Y'
# assert_extended_key 90 'Z'
# assert_extended_key 123 '{'
# assert_extended_key 124 '|'
# assert_extended_key 125 '}'

# assert_key 'Escape [105;5u' 'C-i'
# assert_key 'Escape [73;5u' 'C-I'

# assert_key 'Escape [109;5u' 'C-m'
# assert_key 'Escape [77;5u' 'C-M'

# assert_key 'Escape [91;5u' 'C-['
assert_key 'Escape [123;5u' 'C-{'

# assert_key 'Escape [64;5u' 'C-@'

assert_key 'Escape [32;2u' 'S-Space'
# assert_key 'Escape [32;6u' 'C-S-Space'

assert_key 'Escape [9;5u' 'C-Tab'
assert_key 'Escape [1;5Z' 'C-S-Tab'

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

exit $exit_status
