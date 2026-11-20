#!/usr/bin/env bash
# Boots a built game headless and checks it came up whole: the version it says, a world
# generated and listening, no errors, player data written outside the package, and the
# game's TypeScript mods loaded through terra's sidecar. CI runs it on every platform's
# package; it runs the same here:
#   apps/game/scripts/release/smoke-server.sh <executable> <version>
# Portable to bash 3.2 (macOS) and Git Bash (Windows): no `timeout`, no mapfile.
set -euo pipefail

exe="$1"
version="$2"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export TERRAVERSE_HOME="$work/home"

fail() { echo "smoke: $1" >&2; exit 1; }

"$exe" --version >"$work/version.log" 2>&1 || fail "--version exited $?"
cat "$work/version.log"
grep -qF "$version" "$work/version.log" || fail "--version doesn't say $version"

# The server runs until stopped: give it 20 s to make a world and listen, then stop it.
"$exe" --server --gameid terrafirma --world "$work/world" --world-code TF24.dGVycmE \
	--port 30500 >"$work/boot.log" 2>&1 &
pid=$!
for _ in $(seq 1 40); do
	grep -q 'blocks from' "$work/boot.log" "$TERRAVERSE_HOME/logs/debug.txt" 2>/dev/null \
		&& grep -q 'listening on' "$work/boot.log" && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 0.5
done
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
cat "$work/boot.log"

grep -q 'listening on' "$work/boot.log" || fail "the server didn't start"
! grep -qF 'ERROR[' "$work/boot.log" || fail "errors booting a world"
test -f "$TERRAVERSE_HOME/logs/debug.txt" || fail "player data didn't leave the package"
grep -q 'blocks from' "$work/boot.log" "$TERRAVERSE_HOME/logs/debug.txt" \
	|| fail "the game's TypeScript mods didn't load"
echo "smoke: ok"
