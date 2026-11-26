#!/usr/bin/env bash
# An atlas's seams, crossed headless (.agents/planning/sprint-30-atlas.md, phase 3):
# walkers, water and sand over the ring wrap, band seams, the cap edge and a
# shell floor, on the default world laid 1:1 (apps/game/tests/probe).
# Builds the engine first.
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build
probe_world terra "$probe_atlas_default"
probe_run atlas-seams 300 'ATLAS .*'
probe_zero_failed "atlas seam" 'ATLAS pass'
