#!/usr/bin/env bash
# The world's matter on a live server: a clay fire pit with kindling under a
# log, lit as the fire drill lights it, run for 30 simulated minutes. The
# kindling must burn, flames must rise, reactions must be seen, and the
# awake blocks must save. Builds the engine first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
bash "$root/scripts/build.sh" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/matter-check/probe" "$world/worldmods/probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = singlenode\nseed = 1\n[end_of_params]\n' > "$world/map_meta.txt"
port=$((30700 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 | tee "$work/server.txt" | grep -oE 'MATTER .*' > "$work/log.txt" || true
cat "$work/log.txt"
python3 -I - "$work" <<'PY'
import sys, os, re
work = sys.argv[1]
log = open(f"{work}/log.txt").read()
bad = []
end = re.search(r"MATTER end kindling (\S+) log (\S+) hottest (\d+) awake (\d+)", log)
seen = re.search(r"MATTER seen (\S*)", log)
if not end:
    bad.append("the probe never finished (see the server's log)")
    print(open(f"{work}/server.txt").read()[-3000:])
else:
    kindling, logblock, hottest = end.group(1), end.group(2), int(end.group(3))
    if kindling == "tf_matter:kindling": bad.append("the kindling never burned")
    if hottest < 800: bad.append(f"no flame: the hottest block reached only {hottest} K")
    reactions = set((seen.group(1) if seen else "").split(","))
    if not reactions & {"wood_burns", "methane_burns", "carbon_monoxide_burns"}: bad.append(f"nothing burned (seen: {sorted(reactions)})")
soil = re.search(r"MATTER soil (\S+) fill ([\d.]+) burned (\S+)", log)
if not soil: bad.append("the soil family wasn't probed")
elif soil.group(1) != "silica" or soil.group(3) != "true": bad.append(f"peat should be mostly quartz and burn: {soil.group(0)}")
if not os.path.exists(f"{work}/world/matter.bin"): bad.append("matter.bin wasn't saved")
print("matter check: " + ("ok" if not bad else "FAILED"))
for b in bad: print("  " + b)
sys.exit(1 if bad else 0)
PY
