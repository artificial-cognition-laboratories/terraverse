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
check() { # <code> <atlas> [body]
work="$(mktemp -d)"
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/far-check/probe" "$world/worldmods/far_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = terra\nmgterra_code = %s\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$1" "$2" > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
FAR_BODY="${3:-}" timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 | grep -oE '(FAR|ERROR).*' | tee "$work/log.txt"

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
rm -rf "$work"
}

# TF16.dGVycmE: the default size, 16384 round: an atlas of radius 8192 / 2π, the standard layout
check TF16.dGVycmE "atlas-v2 radius=2607.5945876176133 segments=24 capLatitude=85 core=651.8986469044033 rounding=0.95 height=256"
# size 157088, large enough for the Moon to be stood on: the far view on another body
check TF16.dGVycmE.AgKAqoCAAG1mgICAgICAgICCVQ "atlas-v2 radius=25001.331700419654 segments=24 capLatitude=85 core=6250.3329251049136 rounding=0.94999999999999996 height=256" moon
# a world facing its star, its Moon held to it: the climate round the star's point, the far view and the clock
check TF16.dGVycmE.AQKAqoCAAG1mgICAgICAgICCVQ.AgVlYXJ0aAVFYXJ0aICAgP8TAAAAACIrBFmZgAAABG1vb24ETW9vboCAgAAIkTdQxAJYBFkNgAAA "atlas-v2 radius=10000.023384349968 segments=24 capLatitude=85 core=2500.005846087492 rounding=0.95 height=256"
# a double planet, on its other half: as much a world as the one you start on
check TF16.dGVycmE.AQKAqoCABG1mgICAgICAgICCVQ "atlas-v2 radius=10000.023384349968 segments=24 capLatitude=85 core=2500.005846087492 rounding=0.95 height=256" b7
# two suns, the seed's: the world their combined light warms
check TF16.dGVycmE.AQKAqoCAA21mgICAgICAgICCVQ "atlas-v2 radius=10000.023384349968 segments=24 capLatitude=85 core=2500.005846087492 rounding=0.95 height=256"
# and a close pair the code chooses, round the home system: Earth under both
check TF16.dGVycmE.AQKAqoCAAG1mgICAgICAgICCVQEBgGk "atlas-v2 radius=10000.023384349968 segments=24 capLatitude=85 core=2500.005846087492 rounding=0.95 height=256"
