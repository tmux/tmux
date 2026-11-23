#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
bench_redraw 120 120 40 -s top-120x40
