#!/usr/bin/env bash
# The check before a commit (a minute or two): the libraries' quick tests,
# the engine's unit modules and the UI's types. CI runs the rest (the slow
# tests, a headless world, the check:* harnesses).
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
bash "$root/apps/game/tests/run/library.sh" hot
bash "$root/apps/game/tests/run/engine.sh"
bun run --cwd "$root/apps/game/src/client/app/shell/interface" typecheck
