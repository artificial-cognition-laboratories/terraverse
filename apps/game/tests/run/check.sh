#!/usr/bin/env bash
# The check before a commit (a minute or two): the libraries' quick tests,
# the engine's unit modules and the UI's types. CI runs the rest (the slow
# tests, a headless world, the check:* harnesses).
#   bun check                    all of them
#   bun check --changed [base]   only what uncommitted changes reach (or everything since base)
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
if [[ "${1:-}" == --changed ]]; then exec bash "$root/apps/game/tests/run/test.sh" --hot "$@"; fi
bash "$root/apps/game/tests/run/library.sh" hot
bash "$root/apps/game/tests/run/engine.sh"
bun run --cwd "$root/apps/game/src/client/app/shell/interface" typecheck
