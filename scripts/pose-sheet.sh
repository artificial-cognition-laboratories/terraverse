#!/usr/bin/env bash
# Contact sheets of the figure's moves: bun pose-sheet [out-dir] (default .output/pose-sheet)
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-.output/pose-sheet}"
mkdir -p "$out"
g++ -std=c++17 -O1 -Iapps/game/src -Iapps/game/src/engine -Iapps/game/library -Iapps/game/library/world -Iapps/game/library/space -Iapps/game/library/matter -Iapps/game/library/life -Iapps/game/library/geometry -Iapps/game/library/vendors/irr/include -Iapps/game/library/vendors/jsoncpp \
	apps/game/library/physics/pose.cpp apps/game/library/physics/reach.cpp apps/game/library/physics/walk.cpp apps/game/library/physics/skeleton.cpp \
	scripts/pose-sheet/sheet.cpp -o "$out/sheet"
"$out/sheet" "$out"
