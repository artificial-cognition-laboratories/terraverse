#!/usr/bin/env bash
# The wiki's demos, end to end: every spec the page would make (reactions
# and Lab demos, from ui/src/wiki/demos.ts) run in the engine's sandbox for
# a simulated minute. Fails if any can't be made or stepped.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
g++ -std=c++17 -O2 -I"$root/apps/game" -I"$root/apps/game/src" -I"$root/apps/game/src/engine" -I"$root/apps/game/library" -I"$root/apps/game/library/world" -I"$root/apps/game/library/space" -I"$root/apps/game/library/matter" -I"$root/apps/game/library/life" -I"$root/apps/game/library/geometry" -I"$root/apps/game/library/vendors/irr/include" -I"$root/apps/game/library/vendors/jsoncpp" "$root/apps/game/library/geometry/shape/melt.cpp" "$root"/apps/game/library/matter/matter/*.cpp "$root"/apps/game/library/matter/sandbox/*.cpp \
	"$root/apps/game/library/vendors/jsoncpp/jsoncpp.cpp" "$root/apps/game/tests/checks/wiki/run.cpp" -o "$work/run"
bun "$root/apps/game/tests/checks/wiki/specs.ts" > "$work/specs.json"
"$work/run" "$root/apps/game/assets/matter" "$root/apps/game/assets/game/palette.json" < "$work/specs.json"
