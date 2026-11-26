#!/usr/bin/env bash
# A world's bodies, headless (.agents/planning/sprint-40-bodies.md): a 250k home and
# its moon in one window; a mark on each kept as a place, both bodies'
# tiles pushed out of the map and found again by their places; the moon's
# sky, surface and air its own, its blocks in its own database
# (apps/game/tests/probe). Takes ~70 s (the pager's hold, twice).
# Builds the engine first.
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build background
probe_world terra "$probe_atlas_250k"
probe_run bodies 300 'BODIES .*'

stored=yes
[ -s "$world/bodies/moon/map.sqlite" ] || { echo "BODIES FAILED: the moon's blocks aren't in bodies/moon/map.sqlite"; stored=no; }
grep -qx 'BODIES pass' "$work/log.txt" && [ "$stored" = yes ] && echo "bodies check: ok" || probe_said bodies FAILED
