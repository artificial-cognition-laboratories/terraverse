#!/usr/bin/env bash
# The small streams scored (scripts/stream-check/probe.cpp): breaks by their
# cause in channels.cpp, turns, downhill, banks and ridges, a table a level a
# region, and a picture a region in .output/stream-check (red a break, yellow
# a sharp turn, magenta uphill, a ridge or over a bank; read()'s water blue).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/.output/stream-check"
mkdir -p "$out"
bash "$root/scripts/terragen-tool.sh" "$out/probe" "$root/scripts/stream-check/probe.cpp"
"$out/probe" "$out" | tee "$out/report.txt"
