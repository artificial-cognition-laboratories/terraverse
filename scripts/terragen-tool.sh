#!/usr/bin/env bash
# Builds a standalone tool against terragen: `scripts/terragen-tool.sh <out>
# <tool.cpp>... `. terragen's objects are kept in .output/terragen-tools, so
# after a change only what changed compiles, on every core.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"; shift
make -s -j"$(nproc)" -f "$root/scripts/terragen-tool.mk" ROOT="$root" OBJ="$root/.output/terragen-tools" OUT="$out" TOOL="$*"
