#!/usr/bin/env bash
# A planet's window, headless (.agents/planning/sprint-39-tiles.md): a planet of
# radius 250k, far more than the map holds at once; a mark made at the
# spawn, the spawn's tile pushed out of the map, then asked for again: the
# same cell, the mark kept (apps/game/tests/probe). Takes ~70 s (the
# pager's hold, twice). Builds the engine first.
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
nice -n 19 ionice -c 3 bash "$root/apps/game/scripts/build.sh" -j 3 > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\n' > "$world/world.mt"
atlas="atlas-v2 radius=250000 segments=24 capLatitude=85 core=62500 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = TF24.dGVycmE\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$atlas" > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
timeout 300 "$root/.output/bin/terraverse" --server --config /dev/null --world "$world" --probe window --port "$port" 2>&1 \
	| tee "$work/server.txt" | grep -oE 'WINDOW .*' | tee "$work/log.txt" || true

if grep -qx 'WINDOW pass' "$work/log.txt"; then
	echo "window check: ok"
else
	echo "window check: FAILED; the server said:"
	grep -E "ERROR|error" "$work/server.txt" | head -20
	exit 1
fi
