#!/usr/bin/env bash
# The far view against the world: generate areas of a terra world headless
# and check, column by column, that core.terra.surface (what the far view
# draws) is what the generator made: the ground's block and height and the
# water over it, in the middle of regions of an atlas planet. (Trees are
# planted per map chunk, which a place's column isn't: surface has none on
# a planet.) Builds the engine first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/far-check/probe" "$world/worldmods/far_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
# TF9.dGVycmE: the default size, 16384 round: an atlas of radius 8192 / 2π, the standard layout
atlas="atlas-v2 radius=2607.5945876176133 segments=24 capLatitude=85 core=651.8986469044033 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = TF9.dGVycmE\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$atlas" > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 | grep -oE '(FAR|ERROR).*' | tee "$work/log.txt"

python3 -I - "$work/log.txt" <<'PY'
import sys
line = next((l for l in open(sys.argv[1]) if l.startswith("FAR columns")), None)
if not line:
    sys.exit("far check: no result (did the server start?)")
f = line.split()
n = dict(zip(f[1::2], f[2::2]))
columns = int(n["columns"])
ok = True
for what, need in (("ground", 0.9999), ("water", 0.9999)):
    share = int(n[what]) / columns
    print(f"far check: {what} {int(n[what])} of {columns} columns, {share:.3%} (need {need:.2%})")
    ok &= share >= need
sys.exit(0 if ok else "far check: FAILED")
PY
