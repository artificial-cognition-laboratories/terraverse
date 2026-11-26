#!/usr/bin/env bash
# The debug sphere, generated headless (planet_atlas_debug; sprint 30): the
# default world's atlas with no terrain, every cell painted by what it is;
# boxes where things meet, every node checked (apps/game/tests/probe).
# Builds the engine first.
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build
probe_world terra "$probe_atlas_default" "planet_atlas_debug = true"
probe_run atlas-debug 300 'ATLAS .*'
probe_zero_failed "atlas debug" 'ATLAS pass'
