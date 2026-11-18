#!/usr/bin/env bash
# terragen's erosion core against fastscapelib on a few worlds at once
# (scripts/terragen-check/oracle.cpp; sprint 116): fails if any drifts from
# the reference past its gate. Worlds with water: a dry body's land is closed
# basins everywhere, each spilling over a pass where either side's an equal
# way down; the two models choose differently and wander apart (and the game
# erodes a dry body a round or two at most).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
codes=(TF24.dGVycmE TF24.YmZ2bmVmZzQ TF24.Y3gyMzlqNHY TF24.dGVycmE.AgKAqoCAAP9A2U2ApoCAgICCVQ)
# build once (the first run builds; the rest only run)
bash "$root/scripts/terragen-oracle.sh" --build-only
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
pids=()
for code in "${codes[@]}"; do
	"$root/.output/oracle/oracle" "$code" - 200 > "$work/$code.txt" 2>&1 & pids+=($!)
done
failed=0
for k in "${!codes[@]}"; do
	wait "${pids[$k]}" || failed=1
	grep -E '^(ok|FAIL) ' "$work/${codes[$k]}.txt" || { cat "$work/${codes[$k]}.txt"; failed=1; }
done
exit $failed
