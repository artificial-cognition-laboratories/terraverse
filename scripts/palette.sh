#!/usr/bin/env bash
# The game's baked palette (apps/game/game/palette.json, far/baked.h): what
# the generator places and how it looks from afar, exported from the game's
# own content and textures (luanti --export-palette: a scratch world's mods
# loaded, textures on the NULL video driver, no window). `--check` exports
# afresh and fails if the committed file differs: a block, a texture or a
# species changed without the palette being made again (bun palette).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
bash "$root/scripts/stage-runtime.sh" > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
# any terra world: the palette is the game's, not a world's
atlas="atlas-v2 radius=2607.5945876176133 segments=24 capLatitude=85 core=651.8986469044033 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = TF16.dGVycmE\nseed = 1\nchunksize = 1\nplanet_atlas = %s\n[end_of_params]\n' "$atlas" > "$world/map_meta.txt"

env -u DISPLAY -u WAYLAND_DISPLAY timeout 120 "$root/.output/bin/luanti" --config /dev/null --world "$world" \
	--gameid terrafirma --export-palette "$work/palette.json" > "$work/log.txt" 2>&1 || {
	echo "palette: the export failed:"
	grep -E "ERROR|rror" "$work/log.txt" | head -20
	exit 1
}

committed="$root/apps/game/game/palette.json"
if [ "${1:-}" = "--check" ]; then
	if ! cmp -s "$work/palette.json" "$committed"; then
		echo "palette check: apps/game/game/palette.json is out of date with the game's content (run: bun palette)"
		diff <(python3 -I -m json.tool "$committed" 2>/dev/null || true) <(python3 -I -m json.tool "$work/palette.json") | head -20
		exit 1
	fi
	echo "palette check: ok"
else
	cp "$work/palette.json" "$committed"
	echo "palette: wrote apps/game/game/palette.json"
fi
