#!/usr/bin/env bash
# terragen end to end: generate a world with the terra map generator
# (headless), probe a grid of columns, and check each against terragen built
# standalone. Every surface height and top block must agree, and so must what
# core.terra (calendar, sun, climate) says at each column. Then the unit
# tests. Builds the engine first (bin/luanti).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null # the engine must match the source the predictor is built from
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
code="TF7.dGVycmE"

g++ -std=c++17 -O1 -I"$root/apps/game/src" "$root"/apps/game/src/terragen/*.cpp "$root/scripts/terragen-check/predict.cpp" -o "$work/predict"
world="$work/world"
mkdir -p "$world/worldmods"
"$work/predict" "$code" "$world/points.txt"
cp -r "$root/scripts/terragen-check/probe" "$world/worldmods/probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
# TF7.dGVycmE: the default size, 16384 round: an atlas of radius 8192 / 2π, the standard layout
atlas="atlas-v2 radius=2607.5945876176133 segments=24 capLatitude=85 core=651.8986469044033 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = %s\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$code" "$atlas" > "$world/map_meta.txt"

port=$((30200 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout 180 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 | tee "$work/server.txt" | grep -oE '(MAP|PROBE|TERRA|WEATHER|ACT) .*' > "$work/log.txt" || true
grep '^PROBE' "$work/log.txt" > "$work/probed.txt" || true
# predicted for the places the probed nodes actually are
"$work/predict" --columns "$code" "$work/log.txt" > "$work/predicted.txt"
"$work/predict" --terra "$code" "$work/log.txt" > "$work/terra-predicted.txt"

python3 -I - "$work" <<'PY'
import sys
work = sys.argv[1]
def table(name):
    return {l.split()[1]: l.split()[2:] for l in open(f"{work}/{name}")}
predicted, probed = table("predicted.txt"), table("probed.txt")
# each terragen biome's top block, and its bed under rivers and lakes (games/terrafirma/mods/tf_world/biomes.lua);
# 20 blocks down, the rock, ore or cave terragen's geology decided
TOP = {"deep": "gravel", "ocean": "sand", "seaice": "gravel", "beach": "sand", "desert": "sand",
       "savanna": "dry_grass", "grassland": "grass", "forest": "grass", "jungle": "grass",
       "taiga": "grass", "tundra": "tundra", "rock": "stone", "snow": "snow",
       "monsoon": "grass", "mediterranean": "dry_grass"}
BED = {"rock": "gravel", "snow": "gravel"}
bad, kinds = [], {}
for key, (terrain, water, kind, biome, deep) in predicted.items():
    kinds[kind] = kinds.get(kind, 0) + 1
    got = probed.get(key)
    if not got:
        bad.append(f"{key}: not probed"); continue
    gy, node, gwater, gdeep = got
    if kind == "lava":
        expect = None  # a crater's floor: whatever the rim's rock is
    elif kind in ("river", "lake") and int(water) > int(terrain):
        expect = (f"tf_world:{BED.get(biome, 'sand')}", "tf_world:placer_gravel")
    elif biome == "rock":
        expect = None  # bare peaks: the bedrock shows
    else:
        expect = (f"tf_world:{TOP[biome]}",)
    tree = any(t in node for t in ("log", "leaves", "needles"))
    if gy != terrain and not tree:
        bad.append(f"{key} {kind}: ground {gy}, expected {terrain}")
    elif gy == terrain and expect and node not in expect:
        bad.append(f"{key} {kind}: top {node}, expected {expect[0]} ({biome})")
    # where a river steps down, flowing water runs a block over the stretch below (a rapid)
    rapid = kind == "river" and int(gwater) == int(water) + 1
    if gwater != water and not tree and not rapid:
        bad.append(f"{key} {kind}: water at {gwater}, expected {water}")
    # a storage column runs straight down its region's grid: beside a seam
    # between regions, 20 down can be no cell at all (seam), nothing to compare
    if gy == terrain and gdeep != deep and not tree and gdeep != "tf_planet:seam":
        bad.append(f"{key} {kind}: 20 down {gdeep}, expected {deep}")
summary = ", ".join(f"{n} {k}" for k, n in sorted(kinds.items()))
print(f"terragen check: {len(predicted) - len({b.split(':')[0] for b in bad})}/{len(predicted)} columns agree ({summary})")
for b in bad: print("  " + b)

# core.terra: the probe logged x z day_count time_of_day, then the answers
def answers(name):
    out = {}
    for l in open(f"{work}/{name}"):
        f = l.split()
        if f[0] != "TERRA": continue
        out[f[1]] = f[2:3] + f[4:] if name == "log.txt" else f[2:]
    return out
said, expected = answers("log.txt"), answers("terra-predicted.txt")
wrong = [f"{k}: core.terra said {said.get(k)}, expected {v}" for k, v in expected.items() if said.get(k) != v]
if not said: wrong.append("core.terra: nothing logged")
print(f"core.terra check: {len(said) - len(wrong)}/{len(said)} columns agree")
for w in wrong: print("  " + w)

# core.terra.weather: sane air everywhere, near the climate, and saved with the world.
# Near: the weather's seasons lag the sun (land a month, sea two) while terragen's
# climate doesn't, so just after a polar summer the air runs up to ~half that
# season's swing (±50 K at 75°) above it; nearer the pole the swing is wider still
import math, os
latitude = {l.split()[1]: float(l.split()[7]) for l in open(f"{work}/log.txt") if l.startswith("TERRA ")}
storm = []
for l in open(f"{work}/log.txt"):
    f = l.split()
    if f[0] != "WEATHER": continue
    t, climate, rain, hpa, cloud = map(float, f[2:7])
    near = 35 if abs(latitude[f[1]]) > 80 else 25
    if not all(math.isfinite(v) for v in (t, climate, rain, hpa, cloud)) or abs(t - climate) > near \
            or rain < 0 or not 500 < hpa < 1100 or not 0 <= cloud <= 1:
        storm.append(f"place {f[1]} day {f[8]} hour {f[9]}: {t:.1f} °C (climate {climate:.1f}), {rain:.2f} mm/h, {hpa:.0f} hPa, cloud {cloud:.2f}")
weathered = sum(1 for l in open(f"{work}/log.txt") if l.startswith("WEATHER"))
if not weathered: storm.append("core.terra.weather: nothing logged")
if not os.path.exists(f"{work}/world/weather.bin"): storm.append("weather.bin wasn't saved")
print(f"weather check: {weathered - len(storm)}/{weathered} columns sane")
for s in storm: print("  " + s)

# the weather acting on the world: what tf_weather did to each column's top, against the weather there
acted, odd, counts = 0, [], {}
for l in open(f"{work}/log.txt"):
    f = l.split()
    if f[0] != "ACT": continue
    acted += 1
    _, key, name, action, now, t, snowing, rain = f
    t, snowing, rain = float(t), snowing == "true", float(rain)
    counts[action] = counts.get(action, 0) + 1
    expect = {"freeze": "tf_weather:ice", "snow": "tf_weather:snow", "thaw": "tf_world:water_source", "melt": "air"}.get(action)
    if expect and now != expect: odd.append(f"place {key} {action}: left {now}")
    if name == "tf_world:water_source" and t < -3 and action != "freeze": odd.append(f"place {key} water at {t:.1f} °C didn't freeze")
    if action == "snow" and not (snowing and rain > 0.05): odd.append(f"place {key} snow settled without snow falling")
if not acted: odd.append("tf_weather: nothing acted on")
print(f"weather on the world: {acted - len(odd)}/{acted} columns right ({', '.join(f'{n} {k}' for k, n in sorted(counts.items()))})")
for o in odd: print("  " + o)
sys.exit(1 if bad or wrong or storm or odd else 0)
PY

"$root/.output/bin/luanti" --test-module TestTerragen 2>&1 | grep -E "Results|failed tests"
