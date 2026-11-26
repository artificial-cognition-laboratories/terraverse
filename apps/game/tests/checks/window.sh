#!/usr/bin/env bash
# A planet's window, headless (.agents/planning/sprint-39-tiles.md): a planet of
# radius 250k, far more than the map holds at once; a mark made at the
# spawn, the spawn's tile pushed out of the map, then asked for again: the
# same cell, the mark kept (apps/game/tests/probe). Takes ~70 s (the
# pager's hold, twice). Builds the engine first.
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build background
probe_world terra "$probe_atlas_250k"
probe_run window 300 'WINDOW .*'
grep -qx 'WINDOW pass' "$work/log.txt" && echo "window check: ok" || probe_said window FAILED
