#!/usr/bin/env bash
# What must hold before a release is tagged, checked here in minutes rather
# than found by CI in half an hour: the engine builds, its tests pass, the UI
# typechecks and builds, and a world boots headless without Lua errors.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"

# The key the game trusts must be the key CI signs with.
grep -qF "$(sed -n 2p scripts/release/update-key.pub)" apps/game/src/release/release.cpp \
	|| { echo "gate: release.cpp's UPDATE_KEY isn't scripts/release/update-key.pub" >&2; exit 1; }
grep -qF "$(sed -n 2p scripts/release/update-key.pub)" scripts/release/install.sh \
	|| { echo "gate: install.sh's PUBLIC_KEY isn't scripts/release/update-key.pub" >&2; exit 1; }
bash scripts/release/install-check.sh

cmake -S . -B .output/build -DRUN_IN_PLACE=TRUE
cmake --build .output/build -j"$(nproc)"
# terra runs the game's TypeScript mods beside the engine; the boot below needs it, as CI builds it.
(cd apps/cli && bun install --frozen-lockfile && bun run build)
bash scripts/stage-runtime.sh
# Tests skipped by name pending their job, as CI's release run skips them.
export TF_TEST_SKIP="$(bash scripts/test/skips.sh)"
./.output/bin/luanti --test-module TestOnline
./.output/bin/luanti --test-module TestRelease
./.output/bin/luanti --test-module TestTerragen
(cd apps/game/ui && bun run typecheck && bun run build)

world=$(mktemp -d)
trap 'rm -rf "$world"' EXIT
set +e
timeout 20 ./.output/bin/luanti --server --gameid terrafirma --world "$world/w" --port 30501 >"$world/boot.log" 2>&1
set -e
grep -q 'listening on' "$world/boot.log" || { cat "$world/boot.log"; echo "gate: the server didn't start" >&2; exit 1; }
if grep -q 'ERROR\[' "$world/boot.log"; then grep 'ERROR\[' "$world/boot.log"; echo "gate: errors booting a world" >&2; exit 1; fi
echo "gate: ok"
