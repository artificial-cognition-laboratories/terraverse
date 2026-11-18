#!/usr/bin/env bash
# The tests in scripts/test/skip.txt, space separated, as TF_TEST_SKIP takes them:
#   TF_TEST_SKIP="$(bash scripts/test/skips.sh)" luanti --test-module TestTerragen
set -euo pipefail
grep -vE '^\s*(#|$)' "$(dirname "$0")/skip.txt" | awk '{print $1}' | tr '\n' ' '
