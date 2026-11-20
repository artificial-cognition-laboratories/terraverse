#!/usr/bin/env bash
# Builds a standalone tool against terragen: `apps/game/library/world/terragen/tools/tool.sh <out>
# <tool.cpp>... ` (TOOL_FLAGS: flags for the tool alone). terragen's objects
# are kept in .output/terragen-tools, so after a change only what changed
# compiles, on every core.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
out="$1"; shift
make -s -j"$(nproc)" -f "$root/apps/game/library/world/terragen/tools/tool.mk" ROOT="$root" OBJ="$root/.output/terragen-tools" OUT="$out" TOOL="$*" EXTRA="${TOOL_FLAGS:-}"
