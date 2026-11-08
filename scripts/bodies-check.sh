#!/usr/bin/env bash
# A world's bodies, headless (planning/sprint-40-bodies.md): a 250k home and
# its moon in one window; a mark on each kept as a place, both bodies'
# tiles pushed out of the map and found again by their places; the moon's
# sky, surface and air its own, its blocks in its own database
# (scripts/bodies-check/probe). Takes ~70 s (the pager's hold, twice).
# Builds the engine first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
nice -n 19 ionice -c 3 cmake --build "$root/.output/build" -j 3 > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/bodies-check/probe" "$world/worldmods/bodies_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
atlas="atlas-v2 radius=250000 segments=24 capLatitude=85 core=62500 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = TF19.dGVycmE\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$atlas" > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 \
	| tee "$work/server.txt" | grep -oE 'BODIES .*' | tee "$work/log.txt" || true

stored=yes
[ -s "$world/bodies/moon/map.sqlite" ] || { echo "BODIES FAILED: the moon's blocks aren't in bodies/moon/map.sqlite"; stored=no; }
if grep -qx 'BODIES pass' "$work/log.txt" && [ "$stored" = yes ]; then
	echo "bodies check: ok"
else
	echo "bodies check: FAILED; the server said:"
	grep -E "ERROR|error" "$work/server.txt" | head -20
	exit 1
fi
