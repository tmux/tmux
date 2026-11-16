#!/bin/sh

. ./input-common.inc

# Exercise expansion by VS16, a skin tone modifier and a regional indicator.
for kind in variation modifier flag; do
	case "$kind" in
	variation)
		combine_seq='\342\234\224\357\270\217'
		text='✔️'
		;;
	modifier)
		combine_seq='\342\230\235\360\237\217\273'
		text='☝🏻'
		;;
	flag)
		combine_seq='\360\237\207\254\360\237\207\247'
		text='🇬🇧'
		;;
	esac

	start_pane "$kind-overwrite" 10 3 "A\347\225\214BC\r${combine_seq}X"
	check_capture "$kind-overwrite" "${text}XBC"
	check_cursor "$kind-overwrite" '3,0'
	check_raw_matches "$kind-overwrite" \
	    "C 0,0 data=\(2,[0-9]+,$text\) flags=NONE\[0\]" \
	    'C 0,1 .*flags=PADDING\[[0-9a-f]+\]' \
	    'C 0,2 data=\(1,1,X\) flags=NONE\[0\]'

	start_pane "$kind-edge" 5 3 "abcd${combine_seq}X"
	check_capture "$kind-edge" "abcd
${text}X"
	check_cursor "$kind-edge" '3,1'
	check_raw_matches "$kind-edge" \
	    'L 0 \(0\) flags=.*WRAPPED' \
	    'C 0,4 data=\(1,1, \)' \
	    "C 1,0 data=\(2,[0-9]+,$text\) flags=NONE\[0\]" \
	    'C 1,1 .*flags=PADDING\[[0-9a-f]+\]' \
	    'C 1,2 data=\(1,1,X\) flags=NONE\[0\]'

	start_pane "$kind-fit" 5 3 "abc${combine_seq}X"
	check_capture "$kind-fit" "abc${text}
X"
	check_cursor "$kind-fit" '1,1'

	start_pane "$kind-scroll" 5 2 "11111\r\nabcd${combine_seq}X"
	check_capture "$kind-scroll" "abcd
${text}X"
	check_cursor "$kind-scroll" '3,1'

	start_pane "$kind-region" 5 4 \
	    "top\033[2;1Hold\033[4;1Hend\033[2;3r\033[3;1Habcd${combine_seq}X"
	check_capture "$kind-region" "top
abcd
${text}X
end"
	check_cursor "$kind-region" '3,2'

	start_pane "$kind-insert" 10 3 "ABCDE\r\033[4h${combine_seq}X"
	check_capture "$kind-insert" "${text}XABCDE"
	check_cursor "$kind-insert" '3,0'

	start_pane "$kind-insert-edge" 5 3 \
	    "\033[2;1HABCDE\033[1;1Habcd\033[4h${combine_seq}X"
	check_capture "$kind-insert-edge" "abcd
${text}XAB"
	check_cursor "$kind-insert-edge" '3,1'

	start_pane "$kind-nowrap" 5 3 "\033[?7labc${combine_seq}"
	check_capture "$kind-nowrap" "abc${text}"
	check_cursor "$kind-nowrap" '4,0'
	check_raw_matches "$kind-nowrap" \
	    "C 0,3 data=\(2,[0-9]+,$text\) flags=NONE\[0\]" \
	    'C 0,4 .*flags=PADDING\[[0-9a-f]+\]'
done

# A two-column cluster cannot fit in a one-column pane. Keep its narrow base.
start_pane one-column 1 3 '\342\234\224\357\270\217'
check_capture one-column '✔'
check_cursor one-column '1,0'
check_raw_no_matches one-column 'data=\(2,'

# Without wrapping, even a wide cell filling the pane leaves the cursor inside.
start_pane two-columns-nowrap 2 3 '\033[?7l\342\234\224\357\270\217'
check_capture two-columns-nowrap '✔️'
check_cursor two-columns-nowrap '1,0'

exit $exit_status
