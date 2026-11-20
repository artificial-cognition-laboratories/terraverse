#!/usr/bin/env bash
# terragen's erosion beside fastscapelib's, from the same land on the same
# cells (apps/game/library/world/terragen/tools/oracle.cpp):
#   bash apps/game/library/world/terragen/tools/oracle.sh <code> <out.png|-> [rounds]   (or --build-only)
# fastscapelib is GPL-3: it's fetched into .output/oracle (ignored) at the
# versions it pins, built into this local tool only, and never shipped.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
deps="$root/.output/oracle"
fetch() { # <repo> <dir> <tag>
	if [ ! -d "$deps/$2" ]; then
		git clone -q --depth 1 --branch "$3" "https://github.com/$1.git" "$deps/$2"
	fi
}
mkdir -p "$deps"
fetch fastscape-lem/fastscapelib fastscapelib v0.3.0
fetch xtensor-stack/xtl xtl 0.8.0
fetch xtensor-stack/xtensor xtensor 0.26.0
TOOL_FLAGS="-std=c++20 -w -I$deps/fastscapelib/include -I$deps/xtensor/include -I$deps/xtl/include" \
	"$root/apps/game/library/world/terragen/tools/tool.sh" "$deps/oracle" "$root/apps/game/library/world/terragen/tools/oracle.cpp"
[ "${1:-}" = --build-only ] && exit 0
exec "$deps/oracle" "$@"
