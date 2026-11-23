#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
bench_redraw 35 240 240 less-240x240
