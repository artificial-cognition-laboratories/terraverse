#!/usr/bin/env bash
# Digging, placing and what falls, on a live server (sprint 119: native, were Lua's): a
# block dug lies there as itself, a drop that's another block, placing, a flower coming off
# with its ground, sand falling and landing, drops merging (--probe interact: src/probe).
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
bash "$root/apps/game/scripts/build.sh" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = singlenode\nseed = 1\n[end_of_params]\n' > "$world/map_meta.txt"
port=$((30700 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout 120 "$root/.output/bin/terraverse" --server --config /dev/null --world "$world" --probe interact --port "$port" 2>&1 \
	| tee "$work/server.txt" | grep -oE 'INTERACT .*' | tee "$work/log.txt" || true
line="$(grep -E '^INTERACT pass' "$work/log.txt" || true)"
if [ -z "$line" ]; then
	echo "interact check: no result; the server said:"
	grep -E "ERROR|error" "$work/server.txt" | head -20
	exit 1
fi
fail="$(echo "$line" | awk '{print $5}')"
[ "$fail" = 0 ] && echo "interact check: ok" || { echo "interact check: FAILED"; exit 1; }
