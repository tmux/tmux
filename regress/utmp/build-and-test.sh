#!/bin/sh
# Run explicitly from any directory: sh regress/utmp/build-and-test.sh
# Requires a generated configure (run autogen.sh first), a C toolchain, make,
# pkg-config and tmux's normal build dependencies. JOBS defaults to 2.
# Builds stay under ignored regress/logs/utmp-build; no installed tmux is used.
# The enabled build links ONLY our static libutempter.a, never libutempter.so.
# configure/build output is retained in each variant's directory.

set -eu
LC_ALL=C
export LC_ALL
here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
source=$(CDPATH='' cd -- "$here/../.." && pwd)
build=$source/regress/logs/utmp-build

if [ -f "$source/config.status" ]; then
	echo "Out-of-tree builds require an unconfigured source tree" >&2
	exit 1
fi
[ -x "$source/configure" ] || {
	echo "Run autogen.sh in $source first" >&2
	exit 1
}
# Reject ambient link/compiler flags: they could select the real library.
unset LIBS LDFLAGS CPPFLAGS CFLAGS LD_PRELOAD DYLD_INSERT_LIBRARIES
mkdir -p "$build/fixture" "$build/enabled" "$build/disabled"
cc=${CC:-cc}
"$cc" -Wall -Wextra -Werror -I"$here" -c "$here/utempter.c" \
    -o "$build/fixture/utempter.o"
ar crs "$build/fixture/libutempter.a" "$build/fixture/utempter.o"

for variant in enabled disabled; do
	echo "Building $variant (logs: $build/$variant)"
	(
		cd "$build/$variant"
		if [ "$variant" = enabled ]; then
			CPPFLAGS="-I$here" LIBS="$build/fixture/libutempter.a" \
			    "$source/configure" --enable-utempter \
			    >configure.log 2>&1
		else
			"$source/configure" --disable-utempter \
			    >configure.log 2>&1
		fi
		make clean >build.log 2>&1
		make -j "${JOBS:-2}" >>build.log 2>&1
	) || {
		echo "Build failed; inspect $build/$variant/{configure,build}.log" >&2
		exit 1
	}
	if grep -i 'warning:' "$build/$variant/build.log"; then
		echo "Compiler warnings in $variant build" >&2
		exit 1
	fi
done

for variant in enabled disabled; do
	UTMP_FIXTURE_ZERO_SUCCESS=0 \
	    sh "$here/lifecycle.sh" "$build/$variant" "$variant"
done
UTMP_FIXTURE_ZERO_SUCCESS=1 \
    sh "$here/lifecycle.sh" "$build/enabled" enabled
printf 'Verified binaries:\n%s\n%s\n' \
    "$build/enabled/tmux" "$build/disabled/tmux"
