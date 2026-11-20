#!/usr/bin/env bash
# The land's checks together, after a generation change: the engine built
# once, then side by side, each one's report printed whole, in order.
#   (default)  check:terragen, its unit tests, check:far's every world: the gate
#   --quick    check:terragen's probe and the default world's far view alone,
#              while tuning (no unit tests: the golden and the network's are
#              the gate's)
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
quick=""
[[ "${1:-}" == "--quick" ]] && quick=1
bash "$root/apps/game/scripts/build.sh" -j > /dev/null

logs="$(mktemp -d)"
trap 'rm -rf "$logs"' EXIT
names=()
start() { # <name> <command>...
	local k=${#names[@]}
	names+=("$1"); shift
	( if "$@" > "$logs/$k.txt" 2>&1; then echo 0; else echo 1; fi > "$logs/$k.status" ) &
}
export BUILT=1
start "check:terragen" env NOUNIT=1 bash "$root/apps/game/tests/checks/terragen.sh"
if [[ -n "$quick" ]]; then
	start "check:far (default world)" env FAR_FIRST=1 bash "$root/apps/game/tests/checks/far.sh"
else
	start "terragen's tests" bash "$root/apps/game/tests/run/library.sh" hot terragen
	start "check:far" bash "$root/apps/game/tests/checks/far.sh"
fi
wait

failed=0
for k in "${!names[@]}"; do
	if [[ "$(cat "$logs/$k.status")" == 0 ]]; then
		echo "== ${names[$k]}: passed"
	else
		echo "== ${names[$k]}: FAILED"
		failed=1
	fi
	cat "$logs/$k.txt"
done
exit "$failed"
