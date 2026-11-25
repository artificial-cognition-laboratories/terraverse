#!/usr/bin/env bash
# The tests in apps/game/tests/run/skip.txt, space separated, as TF_TEST_SKIP takes them:
#   TF_TEST_SKIP="$(bash apps/game/tests/run/skips.sh)" terraverse --test-module TestFar
set -euo pipefail
{ grep -vE '^\s*(#|$)' "$(dirname "$0")/skip.txt" || true; } | awk '{print $1}' | tr '\n' ' '
