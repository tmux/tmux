#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
# The recording includes btop's native synchronized updates.
bench_redraw 40 120 40 btop-120x40
