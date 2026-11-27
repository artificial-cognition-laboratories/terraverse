#!/usr/bin/env bash
# The headless probe checks (apps/game/tests/run/checks.txt), side by side, each on its own port:
#   bun run test:checks [name...]   build once, run the named checks (default all)
# Prints each check's time, slowest first, and for each failure its output and the command that
# reproduces it. A check marked skip in checks.txt is listed with its job, not run.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
game="$root/apps/game"
list="$game/tests/run/checks.txt"
[[ -n "${BUILT:-}" ]] || bash "$game/scripts/build.sh" -j"$(getconf _NPROCESSORS_ONLN)" > /dev/null
export BUILT=1

entries="$(sed 's/#.*//' "$list" | awk 'NF')"
if [[ $# -gt 0 ]]; then names=("$@"); else mapfile -t names < <(echo "$entries" | awk '$2 != "skip" {print $1}'); fi
for n in "${names[@]}"; do
	[[ -f "$game/tests/checks/$n.sh" ]] || { echo "test:checks: no check '$n' (tests/checks/$n.sh)" >&2; exit 2; }
done

logs="$(mktemp -d)"
trap 'rm -rf "$logs"' EXIT
start=$SECONDS
i=0
for n in "${names[@]}"; do
	( s=$SECONDS
	  if PROBE_PORT=$((31300 + i)) bash "$game/tests/checks/$n.sh" > "$logs/$n.log" 2>&1; then r=0; else r=$?; fi
	  echo "$r $((SECONDS - s))" > "$logs/$n.status" ) &
	i=$((i + 1))
done
wait
wall=$((SECONDS - start))

for n in "${names[@]}"; do
	read -r r t < "$logs/$n.status"
	printf '%6ds  %s%s\n' "$t" "$n" "$([[ $r == 0 ]] || echo '  FAILED')"
done | sort -rn
failed=0
for n in "${names[@]}"; do
	read -r r _ < "$logs/$n.status"
	[[ $r == 0 ]] && continue
	failed=$((failed + 1))
	echo "== check:$n failed (exit $r): reproduce with  bash apps/game/tests/checks/$n.sh"
	tail -25 "$logs/$n.log"
done
echo "$entries" | awk '$2 == "skip" {$2 = ""; print "   skipped: " $0}'
printf 'test:checks: %d checks, %d failed, %ds wall\n' "${#names[@]}" "$failed" "$wall"
[[ $failed -eq 0 ]]
