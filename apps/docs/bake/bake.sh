#!/usr/bin/env bash
# Rebuilds worlds/ from the generator. Run after a generation change: a code's
# version moves, and the page's codes have to move with it.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
src=$here/../game/src
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
g++ -std=c++17 -O2 -I"$src" "$src"/terragen/*.cpp "$here/bake/bake.cpp" -o "$tmp/bake" -lpthread
out=$here/worlds
rm -f "$out"/*
list=""
for pair in terra:dGVycmE nova:bm92YQ ember:ZW1iZXI hearth:aGVhcnRo arclabs:YXJjbGFicw abc:YWJj; do
	name=${pair%%:*}
	code=TF19.${pair#*:}
	stats=$("$tmp/bake" "$code" "$tmp/$name.ppm")
	magick "$tmp/$name.ppm" -define webp:lossless=true "$out/$name.webp"
	list+="${list:+,}{\"name\":\"$name\",\"code\":\"$code\",\"stats\":$stats}"
done
printf '[%s]\n' "$list" > "$out/worlds.json"
