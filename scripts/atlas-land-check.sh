#!/usr/bin/env bash
# What the generator places is what the far view draws, node for node
# (planning/sprint-32-atlas-lod.md, phase 1): the game's default world on an
# atlas planet, areas emerged where the atlas changes, every node compared
# with terra::AtlasLand (core.terra.land). Only the game's planting and
# flowed liquid may differ (scripts/atlas-land-check/probe). Builds first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/atlas-land-check/probe" "$world/worldmods/atlas_land_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
# TF16.dGVycmE: the default size, 16384 round: radius 8192 / 2π, the standard layout
atlas="atlas-v2 radius=2607.5945876176133 segments=24 capLatitude=85 core=651.8986469044033 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = TF16.dGVycmE\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$atlas" > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 \
	| tee "$work/server.txt" | grep -oE '(LAND|ERROR).*' | tee "$work/log.txt" || true

line="$(grep -E '^LAND nodes' "$work/log.txt" || true)"
if [ -z "$line" ]; then
	echo "atlas land check: no result; the server said:"
	grep -E "ERROR|error" "$work/server.txt" | head -20
	exit 1
fi
echo "$line"
nodes="$(echo "$line" | awk '{print $3}')"
wrong="$(echo "$line" | awk '{print $11}')"
[ "$nodes" -gt 100000 ] && [ "$wrong" = 0 ] && echo "atlas land check: ok" || { echo "atlas land check: FAILED"; exit 1; }
