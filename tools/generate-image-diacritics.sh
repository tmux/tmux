#!/bin/sh

# Generate the Unicode diacritic table used by Kitty image placeholders.
#
# Input must be UnicodeData.txt from Unicode 6.0.0:
# https://www.unicode.org/Public/6.0.0/ucd/UnicodeData.txt
# SHA-256: 90b45b777346bef027556f9c6cb3ea5d7d745bd60d6762855c08d8a22d34f771
#
# The selection follows the public Kitty graphics protocol: nonspacing marks
# with canonical combining class 230, NSM bidi class, no decomposition, minus
# characters which may be combined or normalized in ways unsuitable for a
# row or column index.

if [ "$#" -ne 1 ]; then
	echo "usage: $0 UnicodeData.txt" >&2
	exit 1
fi

awk -F ';' '
BEGIN {
	omit["0300"] = omit["0301"] = omit["0302"] = 1
	omit["0303"] = omit["0304"] = omit["0306"] = 1
	omit["0307"] = omit["0308"] = omit["0309"] = 1
	omit["030A"] = omit["030B"] = omit["030C"] = 1
	omit["030F"] = omit["0311"] = omit["0313"] = 1
	omit["0314"] = omit["0342"] = omit["0653"] = 1
	omit["0654"] = 1
}
$3 == "Mn" && $4 == "230" && $5 == "NSM" && $6 == "" && !omit[$1] {
	point[++count] = $1
}
END {
	if (count != 297) {
		printf "expected 297 diacritics, found %u\n", count > "/dev/stderr"
		exit 1
	}
	print "static const uint32_t kitty_diacritics[] = {"
	for (i = 1; i <= count; i++) {
		if ((i - 1) % 8 == 0)
			printf "\t"
		printf "0x%s", point[i]
		if (i != count)
			printf ","
		if (i == count || i % 8 == 0)
			printf "\n"
		else
			printf " "
	}
	print "};"
}
' "$1"
