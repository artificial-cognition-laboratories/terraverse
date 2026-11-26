#!/usr/bin/env bash
# Digging, placing and what falls, on a live server (sprint 119: native, were Lua's): a
# block dug lies there as itself, a drop that's another block, placing, a flower coming off
# with its ground, sand falling and landing, drops merging (--probe interact: src/probe).
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build
probe_world singlenode
probe_run interact 120 'INTERACT .*'
probe_zero_failed "interact" 'INTERACT pass'
