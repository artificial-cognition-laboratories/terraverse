#!/usr/bin/env bash
# An atlas world, generated headless (.agents/planning/sprint-30-atlas.md, phase 2):
# the game's default world code laid 1:1 on an atlas planet (radius =
# size / 2π), boxes emerged where its storage changes, every node checked
# against the cell it stores (apps/game/tests/probe). Builds the
# engine first.
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build
probe_world terra "$probe_atlas_default"
probe_run atlas-world 300 'ATLAS .*'
probe_zero_failed "atlas world" 'ATLAS pass'
