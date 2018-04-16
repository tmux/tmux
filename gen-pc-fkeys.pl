#! /usr/bin/perl -w
# $XTermId: gen-pc-fkeys.pl,v 1.22 2007/11/30 23:03:55 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2004-2005,2007 by Thomas E. Dickey
# 
#                         All Rights Reserved
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# 
# Except as contained in this notice, the name(s) of the above copyright
# holders shall not be used in advertising or otherwise to promote the
# sale, use or other dealings in this Software without prior written
# authorization.
# -----------------------------------------------------------------------------
#
# Construct a list of function-key definitions corresponding to xterm's
# Sun/PC keyboard.  This uses ncurses' infocmp to obtain the strings (including
# extensions) to modify (and verify).
use strict;

my($max_modifier, $terminfo);
my(@old_fkeys, $opt_fkeys, $min_fkeys, $max_fkeys);
my(%old_ckeys, $opt_ckeys, $min_ckeys, $max_ckeys);
my(%old_ekeys, $opt_ekeys, $min_ekeys, $max_ekeys);

my(@ckey_names);
@ckey_names = (
	'kcud1', 'kcub1', 'kcuf1', 'kcuu1',	# 1 = no modifiers
	'kDN',   'kLFT',  'kRIT',  'kUP',	# 2 = shift
	# make_ckey_names() repeats this row, appending the modifier code
	);
my %ckey_names;
my(@ckey_known);
@ckey_known = (
	'kind',  'kLFT',  'kRIT',  'kri',	# 2 = shift (standard)
	);

my(@ekey_names);
@ekey_names = (
	'khome', 'kend',  'knp',   'kpp',   'kdch1', 'kich1', # 1 = no modifiers
	'kHOM',  'kEND',  'kNXT',  'kPRV',  'kDC',   'kIC',   # 2 = shift
	# make_ekey_names() repeats this row, appending the modifier code
);
my %ekey_names;

$min_fkeys=12;		# the number of "real" function keys on your keyboard
$max_fkeys=64;		# the number of function-keys terminfo can support
$max_modifier=8;	# modifier 1 + (1=shift, 2=alt, 4=control 8=meta)

$min_ckeys=4;		# the number of "real" cursor keys on your keyboard
$max_ckeys=($min_ckeys * ($max_modifier - 1));

$min_ekeys=6;		# the number of "real" editing keys on your keyboard
$max_ekeys=($min_ekeys * ($max_modifier - 1));

$opt_ckeys=2;		# xterm's modifyCursorKeys resource
$opt_ekeys=2;		# xterm's modifyCursorKeys resource
$opt_fkeys=2;		# xterm's modifyFunctionKeys resource
$terminfo="xterm-new";	# the terminfo entry to use

# apply the given modifier to the terminfo string, return the result
sub modify_fkey($$$) {
	my $code = $_[0];
	my $text = $_[1];
	my $opts = $_[2];
	if (not defined($text)) {
		$text = "";
	} elsif ($code != 1) {
		$text =~ s/\\EO/\\E\[/ if ($opts >= 1);

		my $piece = substr $text, 0, length ($text) - 1;
		my $final = substr $text, length ($text) - 1;
		my $check = substr $piece, length ($piece) - 1;
		if ($check =~ /[0-9]/) {
			$code = ";" . $code;
		} elsif ( $check =~ /\[/ and $opts >= 2) {
			$code = "1;" . $code;
		}
		if ( $opts >= 3 ) {
			$code = ">" . $code;
		}
		$text = $piece . $code . $final;
		$text =~ s/([\d;]+)>/>$1/;
	}
	return $text;
}

# compute the next modifier value -
# Cycling through the modifiers is not just like counting.  Users prefer
# pressing one modifier (even if using Emacs).  So first we cycle through
# the individual modifiers, then for completeness two, three, etc.
sub next_modifier {
	my $code = $_[0];
	my $mask = $code - 1;
	if ($mask == 0) {
		$mask = 1;	# shift
	} elsif ($mask == 1) {
		$mask = 4;	# control
	} elsif ($mask == 2) {
		$mask = 3;	# shift+alt
	} elsif ($mask == 4) {
		$mask = 5;	# shift+control
	} elsif ($mask == 5) {
		$mask = 2;	# alt
	}
	# printf ("# next_modifier(%d) = %d\n", $code, $mask + 1);
	return $mask + 1;
}

sub make_ckey_names() {
	my ($j, $k);
	my $min = $min_ckeys * 2;
	my $max = $max_ckeys - 1;

	# printf "# make_ckey_names\n";
	for $j ($min..$max) {
		$k = 1 + substr($j / $min_ckeys, 0, 1);
		$ckey_names[$j] = $ckey_names[$min_ckeys + ($j % $min_ckeys)] . $k;
		# printf "# make %d:%s\n", $j, $ckey_names[$j];
	}
	for $j (0..$#ckey_names) {
		# printf "# %d:%s\n", $j, $ckey_names[$j];
		$ckey_names{$ckey_names[$j]} = $j;
	}
}

sub make_ekey_names() {
	my ($j, $k);
	my $min = $min_ekeys * 2;
	my $max = $max_ekeys - 1;

	# printf "# make_ekey_names\n";
	for $j ($min..$max) {
		$k = 1 + substr($j / $min_ekeys, 0, 1);
		$ekey_names[$j] = $ekey_names[$min_ekeys + ($j % $min_ekeys)] . $k;
		# printf "# make %d:%s\n", $j, $ekey_names[$j];
	}
	for $j (0..$#ekey_names) {
		# printf "# %d:%s\n", $j, $ekey_names[$j];
		$ekey_names{$ekey_names[$j]} = $j;
	}
}

# Read the terminfo entry's list of function keys $old_fkeys[].
# We could handle $old_fkeys[0], but choose to start numbering from 1.
sub readterm($) {
	my $term = $_[0];
	my($key, $n, $str);
	my(@list) = `infocmp -x -1 $term`;

	for $n (0..$#list) {
		chop $list[$n];
		$list[$n] =~ s/^[[:space:]]//;

		$key = $list[$n];
		$key =~ s/=.*//;

		$str = $list[$n];
		$str =~ s/^[^=]+=//;
		$str =~ s/,$//;

		if ( $list[$n] =~ /^kf[[:digit:]]+=/ ) {
			$key =~ s/^kf//;
			# printf "# $n:%s(%d)(%s)\n", $list[$n], $key, $str;
			$old_fkeys[$key] = $str;
		} elsif ( $key =~ /^kc[[:alpha:]]+1/
			or $key =~ /^k(LFT|RIT|UP|DN)\d?/) {
			# printf "# $n:%s(%d)(%s)\n", $list[$n], $key, $str;
			$old_ckeys{$key} = $str;
		} elsif ( defined $ekey_names{$key} ) {
			# printf "# $n:%s(%s)(%s)\n", $list[$n], $key, $str;
			$old_ekeys{$key} = $str;
		}
	}
	# printf ("last index:%d\n", $#old_fkeys);
}

# read the whole terminfo to ensure we get the non-modified stuff, then read
# the part that contains modifiers.
sub read_part($) {
	my $part = $_[0];

	%old_ckeys = ();
	@old_fkeys = ();
	readterm($terminfo);
	readterm($part);
}

sub nameof_ckeys($) {
	my $opts = $_[0];
	my $optname = "xterm+pcc" . ($opts >= 0 ? $opts : "n");
	return $optname;
}

sub generate_ckeys($) {
	my $opts = $_[0];
	my($modifier, $cur_ckey, $index);

	printf "%s|fragment with modifyCursorKeys:%s,\n",
		nameof_ckeys($opts), $opts;

	# show the standard cursor definitions
	$modifier = 1;
	for ($index = 0; $index < $min_ckeys; ++$index) {
		$cur_ckey = $index + ($modifier * $min_ckeys);
		my $name = $ckey_known[$index];
		my $input = $old_ckeys{$ckey_names[$index]};
		my $result = modify_fkey($modifier + 1, $input, $opts);
		printf "\t%s=%s,\n", $name, $result;
		if (defined $old_ckeys{$name}) {
			if ($old_ckeys{$name} ne $result) {
				printf "# found %s=%s\n", $name, $old_ckeys{$name};
			}
		}
	}

	# show the extended cursor definitions
	for ($index = 0; $index < $min_ckeys; ++$index) {
		for ($modifier = 1; $modifier < $max_modifier; ++$modifier) {
			$cur_ckey = $index + ($modifier * $min_ckeys);
			if (defined $ckey_names[$cur_ckey] and
				$ckey_names[$cur_ckey] ne "kLFT" and
				$ckey_names[$cur_ckey] ne "kRIT" ) {
				my $name = $ckey_names[$cur_ckey];
				my $input = $old_ckeys{$ckey_names[$index]};
				my $result = modify_fkey($modifier + 1, $input, $opts);
				printf "\t%s=%s,\n", $name, $result;
				if (defined $old_ckeys{$name}) {
					if ($old_ckeys{$name} ne $result) {
						printf "# found %s=%s\n", $name, $old_ckeys{$name};
					}
				}
			}
		}
	}
}

sub nameof_ekeys($) {
	my $opts = $_[0];
	my $optname = "xterm+pce" . ($opts >= 0 ? $opts : "n");
	return $optname;
}

sub generate_ekeys($) {
	my $opts = $_[0];
	my($modifier, $cur_ekey, $index);

	printf "%s|fragment with modifyCursorKeys:%s,\n",
		nameof_ekeys($opts), $opts;

	for ($index = 0; $index < $min_ekeys; ++$index) {
		for ($modifier = 1; $modifier < $max_modifier; ++$modifier) {
			$cur_ekey = $index + ($modifier * $min_ekeys);
			if (defined $ekey_names[$cur_ekey] ) {
				my $name = $ekey_names[$cur_ekey];
				my $input = $old_ekeys{$ekey_names[$index]};
				my $result = modify_fkey($modifier + 1, $input, $opts);
				printf "\t%s=%s,\n", $name, $result;
				if (defined $old_ekeys{$name}) {
					if ($old_ekeys{$name} ne $result) {
						printf "# found %s=%s\n", $name, $old_ekeys{$name};
					}
				}
			}
		}
	}
}

sub nameof_fkeys($) {
	my $opts = $_[0];
	my $optname = "xterm+pcf" . ($opts >= 0 ? $opts : "n");
	return $optname;
}

sub generate_fkeys($) {
	my $opts = $_[0];
	my($modifier, $cur_fkey);

	printf "%s|fragment with modifyFunctionKeys:%s and ctrlFKeys:10,\n",
		nameof_fkeys($opts), $opts;

	for ($cur_fkey = 1, $modifier = 1; $cur_fkey < $max_fkeys; ++$cur_fkey) {
		my $index = (($cur_fkey - 1) % $min_fkeys);
		if ($index == 0 && $cur_fkey != 1) {
			$modifier = next_modifier($modifier);
		}
		if (defined $old_fkeys[$index + 1]) {
			my $input = $old_fkeys[$index + 1];
			my $result = modify_fkey($modifier, $input, $opts);
			printf "\tkf%d=%s,\n", $cur_fkey, $result;
			if (defined $old_fkeys[$cur_fkey]) {
				if ($old_fkeys[$cur_fkey] ne $result) {
					printf "# found kf%d=%s\n", $cur_fkey, $old_fkeys[$cur_fkey];
				}
			}
		}
	}
}

sub show_default() {
	readterm($terminfo);

	printf "xterm+pcfkeys|fragment for PC-style keys,\n";
	printf "\tuse=%s,\n", nameof_ckeys($opt_ckeys);
	printf "\tuse=%s,\n", nameof_ekeys($opt_ekeys);
	printf "\tuse=%s,\n", nameof_fkeys($opt_fkeys);

	generate_ckeys($opt_ckeys);
	generate_ekeys($opt_ekeys);
	generate_fkeys($opt_fkeys);
}

sub show_nondefault()
{
	my $opts;

	for ($opts = 0; $opts <= 3; ++$opts) {
		if ($opts != $opt_ckeys) {
			read_part(nameof_ckeys($opts));
			generate_ckeys($opts);
		}
	}

	for ($opts = 0; $opts <= 3; ++$opts) {
		if ($opts != $opt_ekeys) {
			read_part(nameof_ekeys($opts));
			generate_ekeys($opts);
		}
	}

	for ($opts = 0; $opts <= 3; ++$opts) {
		if ($opts != $opt_fkeys) {
			read_part(nameof_fkeys($opts));
			generate_fkeys($opts);
		}
	}
}

make_ckey_names();
make_ekey_names();

printf "# gen-pc-fkeys.pl\n";
printf "# %s:timode\n", "vile";
show_default();
show_nondefault();
