#!/usr/bin/env bash
# Builds the engine in .output/build. Two builds at once in one tree corrupt
# each other's objects, so they take turns: the second waits on the lock, then
# finds the first's work done. Arguments go to `cmake --build` (e.g. -j 3).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/.output"
lock="$root/.output/build.lock"
if command -v flock >/dev/null; then
	exec flock "$lock" cmake --build "$root/.output/build" "$@"
fi
# macOS has no flock(1): perl takes the same lock, and the build inherits it.
exec perl -MFcntl=:flock -e 'open(my $f, ">>", shift) or die "build.sh: $!\n"; flock($f, LOCK_EX) or die "build.sh: $!\n"; exec @ARGV or die "build.sh: $!\n"' \
	"$lock" cmake --build "$root/.output/build" "$@"
