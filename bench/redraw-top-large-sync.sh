#!/bin/sh
set -e

. "$(dirname "$0")/common.inc"
bench_redraw 35 240 240 -s top-240x240
