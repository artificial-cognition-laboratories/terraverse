#!/usr/bin/env bash
# Builds the engine in .output/build. Two builds at once in one tree corrupt
# each other's objects, so they take turns: the second waits on the lock, then
# finds the first's work done. Arguments go to `cmake --build` (e.g. -j 3).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/.output"
exec flock "$root/.output/build.lock" cmake --build "$root/.output/build" "$@"
