#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
# The recording includes Neovim's native synchronized updates.
bench_redraw 120 120 40 neovim-120x40
