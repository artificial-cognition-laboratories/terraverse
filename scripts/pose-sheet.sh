#!/usr/bin/env bash
# Contact sheets of the figure's moves: bun pose-sheet [out-dir] (default .output/pose-sheet)
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-.output/pose-sheet}"
mkdir -p "$out"
g++ -std=c++17 -O1 -Iapps/game/src -Ilibrary/irr/include \
	apps/game/src/physics/pose.cpp apps/game/src/physics/reach.cpp apps/game/src/physics/walk.cpp apps/game/src/physics/skeleton.cpp \
	scripts/pose-sheet/sheet.cpp -o "$out/sheet"
"$out/sheet" "$out"
