#!/usr/bin/env bash
# Rebakes the landing's planet pictures (public/home/) from the real generator,
# through terragen's render tool. Run after a generation change: a code's
# version moves, and the page's codes (app/config/home.ts) move with it.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
root=$(git -C "$here" rev-parse --show-toplevel)
tools=$root/apps/game/library/world/terragen/tools
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"$tools/tool.sh" "$tmp/render" "$tools/render.cpp"

version=TF24
# One region of one world, as four layers the landing steps through
# (app/config/home.ts universe.layers): the same crop of each 1536² map (scale 6).
code=$version.dGVycmE
crop=1200x600+150+600
for mode in plates relief rain biomes; do
	"$tmp/render" "$code" "$tmp/$mode.png" 6 "$mode" > /dev/null
	magick "$tmp/$mode.png" -crop "$crop" +repage -quality 90 "$here/public/home/layer-$mode.webp"
	echo "✓ $mode"
done
