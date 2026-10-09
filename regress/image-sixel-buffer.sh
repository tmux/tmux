#!/bin/sh

# Catch a trailing NUL written beyond an exactly full SIXEL output buffer.

PATH=/bin:/usr/bin
export PATH

SIXEL_TEST_DIR=$(mktemp -d) || exit 1
trap 'rm -rf "$SIXEL_TEST_DIR"' 0 1 15
export SIXEL_TEST_DIR
ASAN_OPTIONS=detect_leaks=0
export ASAN_OPTIONS

case $(uname -s) in
Darwin) SIXEL_TEST_LDFLAGS=-Wl,-dead_strip ;;
*) SIXEL_TEST_LDFLAGS=-Wl,--gc-sections ;;
esac
export SIXEL_TEST_LDFLAGS

# Skip when the configured compiler or its runtime cannot use ASan.
printf 'int main(void) { return 0; }\n' >"$SIXEL_TEST_DIR/probe.c"
if ! make -s -C .. -f Makefile -f regress/image-sixel-buffer.mk \
    sixel-buffer-probe >"$SIXEL_TEST_DIR/probe.log" 2>&1; then
	exit 0
fi
if ! "$SIXEL_TEST_DIR/probe" >"$SIXEL_TEST_DIR/probe.log" 2>&1; then
	exit 0
fi

make -s -C .. -f Makefile -f regress/image-sixel-buffer.mk \
    sixel-buffer-test || exit 1
"$SIXEL_TEST_DIR/test" || exit 1
