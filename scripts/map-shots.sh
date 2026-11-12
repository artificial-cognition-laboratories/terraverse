#!/usr/bin/env bash
# The planetarium's map scored against a reference: bun shots:map [out-dir] (default .output/map-shots)
# Renders the ladder (a small planet down to the ground) as the game does, and
# again at twice the size with cells of a pixel (texels of a quarter), box-filtered down: what each
# pixel should be. Prints each rung's RMSE (0..1) and writes a side-by-side sheet.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-.output/map-shots}"
W=1280 H=720 SS=2 REF_CELL=2
rm -rf "$out" && mkdir -p "$out/game" "$out/ref-big" "$out/ref"

render() { # <dir> <w> <h> [cell]
	local conf="$out/$(basename "$1").conf"
	printf 'screen_w = %s\nscreen_h = %s\nfullscreen = false\n' "$2" "$3" > "$conf"
	./.output/bin/luanti --config "$conf" --planetarium-ladder "$1" ${4:+--planetarium-cell "$4"} >/dev/null
}

render "$out/game" "$W" "$H"
render "$out/ref-big" $((W * SS)) $((H * SS)) "$REF_CELL"

printf '%-14s %s\n' rung rmse
for f in "$out"/game/*.png; do
	n=$(basename "$f")
	magick "$out/ref-big/$n" -filter box -resize "${W}x${H}!" "$out/ref/$n"
	# compare exits 1 when the images differ: that's the measurement, not a failure
	rmse=$(compare -metric RMSE "$f" "$out/ref/$n" null: 2>&1 | sed -E 's/.*\(([0-9.e-]+)\).*/\1/' || true)
	printf '%-14s %s\n' "${n%.png}" "$rmse"
	magick "$f" "$out/ref/$n" +append -resize 50% "$out/side-$n"
done
magick "$out"/side-*.png -append "$out/sheet.png"
echo "sheet: $out/sheet.png (game | reference)"
