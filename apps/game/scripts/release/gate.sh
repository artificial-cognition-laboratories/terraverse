#!/usr/bin/env bash
# What must hold before a release is tagged, checked here in minutes rather
# than found by CI in half an hour: the engine builds, its tests pass, the UI
# typechecks and builds, and a world boots headless without Lua errors.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
cd "$root"

# The key the game trusts must be the key CI signs with.
grep -qF "$(sed -n 2p apps/game/scripts/release/update-key.pub)" apps/game/src/online/release/release.cpp \
	|| { echo "gate: release.cpp's UPDATE_KEY isn't apps/game/scripts/release/update-key.pub" >&2; exit 1; }
grep -qF "$(sed -n 2p apps/game/scripts/release/update-key.pub)" apps/game/scripts/release/install.sh \
	|| { echo "gate: install.sh's PUBLIC_KEY isn't apps/game/scripts/release/update-key.pub" >&2; exit 1; }
bash apps/game/scripts/release/install-check.sh

cmake -S apps/game -B .output/build -DRUN_IN_PLACE=TRUE
cmake --build .output/build -j"$(nproc)"
# terra runs the game's TypeScript mods beside the engine; the boot below needs it, as CI builds it.
(cd apps/cli && bun install --frozen-lockfile && bun run build)
bash apps/game/scripts/stage-runtime.sh
# Every unit module, side by side (apps/game/tests/run/engine.sh), skipping by name what
# apps/game/tests/run/skip.txt cites a job for, as CI's release run does. Three modules
# alone let 6 failing tests reach master unseen (2026-11-17).
BUILT=1 bash apps/game/tests/run/engine.sh
(cd apps/game/src/client/app/shell/interface && bun run typecheck && bun run build)

world=$(mktemp -d)
trap 'rm -rf "$world"' EXIT
set +e
timeout 20 ./.output/bin/luanti --server --gameid terrafirma --world "$world/w" --world-code TF24.dGVycmE --port 30501 >"$world/boot.log" 2>&1
set -e
grep -q 'listening on' "$world/boot.log" || { cat "$world/boot.log"; echo "gate: the server didn't start" >&2; exit 1; }
if grep -q 'ERROR\[' "$world/boot.log"; then grep 'ERROR\[' "$world/boot.log"; echo "gate: errors booting a world" >&2; exit 1; fi
echo "gate: ok"
