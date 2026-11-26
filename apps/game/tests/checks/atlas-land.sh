#!/usr/bin/env bash
# What the generator places is what the far view draws, node for node
# (.agents/planning/sprint-32-atlas-lod.md, phase 1): the game's default world on an
# atlas planet, areas emerged where the atlas changes, every node compared
# with terra::AtlasLand (core.terra.land). Only the game's planting and
# flowed liquid may differ (apps/game/tests/probe). Builds first.
set -euo pipefail
source "$(dirname "$0")/lib/probe.sh"

probe_build
probe_world terra "$probe_atlas_default"
probe_run atlas-land 300 '(LAND|ERROR).*'
line="$(probe_line "atlas land" 'LAND nodes')"
echo "$line"
nodes="$(echo "$line" | awk '{print $3}')"
wrong="$(echo "$line" | awk '{print $11}')"
[ "$nodes" -gt 100000 ] && [ "$wrong" = 0 ] && echo "atlas land check: ok" || { echo "atlas land check: FAILED"; exit 1; }
