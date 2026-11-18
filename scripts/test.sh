#!/usr/bin/env bash
# The engine's unit tests, every module in its own process, side by side:
# the same tests as `luanti --run-unittests`, in a fraction of the time.
#   bun run test [Module...]   build once, run the named modules (default all)
# Prints each module's time, slowest first, and for each failure its log and
# the command that reproduces it. Modules in scripts/test/serial.txt time
# themselves against the wall clock, so they run alone, after the rest. Tests in scripts/test/skip.txt are skipped
# by name and listed, each citing the job that tracks it.
set -euo pipefail
# Portable to macOS (bash 5, as its CI runners have): no nproc, no date +%N there.
cpus="$(getconf _NPROCESSORS_ONLN)"
usecs() { local t="${EPOCHREALTIME/[.,]/}"; echo "$t"; }
export -f usecs
root="$(cd "$(dirname "$0")/.." && pwd)"
bin="$root/.output/bin/luanti"
[[ -n "${BUILT:-}" ]] || bash "$root/scripts/build.sh" -j"$cpus" > /dev/null

skips="$(bash "$root/scripts/test/skips.sh")"
export TF_TEST_SKIP="$skips"

if [[ $# -gt 0 ]]; then modules=("$@"); else mapfile -t modules < <("$bin" --list-test-modules 2>&1 | grep -E '^Test'); modules+=(Catch); fi
[[ ${#modules[@]} -gt 0 ]] || { echo "test: no modules listed" >&2; exit 1; }

logs="$(mktemp -d)"
trap 'rm -rf "$logs"' EXIT
start=$(usecs)
# Each process writes its log, its status and its milliseconds. "Catch" is
# the Catch2 tests, which --run-unittests also runs after the modules.
# The module comes as the last argument ($2), not through -I{}: BSD xargs (macOS) caps
# what -I substitutes at 255 bytes, and this script is longer.
run() { # <jobs>, module names on stdin
	xargs -r -n 1 -P "$1" bash -c '
		s=$(usecs)
		if [[ "$2" == Catch ]]; then cmd=(--run-tests); else cmd=(--test-module "$2"); fi
		if "$0" "${cmd[@]}" > "$1/$2.log" 2>&1; then r=0; else r=$?; fi
		echo "$r $(( ($(usecs) - s) / 1000 ))" > "$1/$2.status"
	' "$bin" "$logs"
}
serial="$(sed 's/#.*//' "$root/scripts/test/serial.txt" | awk 'NF {print $1}')"
printf '%s\n' "${modules[@]}" | grep -vxF -f <(echo "$serial" | grep .) | run "$cpus" || true
printf '%s\n' "${modules[@]}" | grep -xF -f <(echo "$serial" | grep .) | run 1 || true
wall=$(( ($(usecs) - start) / 1000000 ))

for m in "${modules[@]}"; do
	read -r r t < "$logs/$m.status"
	printf '%8.1fs  %s%s\n' "$(( t / 1000 )).$(( t % 1000 / 100 ))" "$m" "$([[ $r == 0 ]] || echo '  FAILED')"
done | sort -rn
failed=()
for m in "${modules[@]}"; do
	read -r r _ < "$logs/$m.status"
	[[ $r == 0 ]] && continue
	failed+=("$m")
	echo "== $m failed (exit $r): reproduce with"
	if [[ $m == Catch ]]; then echo "   $bin --run-tests"; else echo "   TF_TEST_SKIP='$skips' $bin --test-module $m"; fi
	grep -E -B3 '\[FAIL\]|Caught|Segmentation|Aborted|terminate' "$logs/$m.log" | tail -40 || tail -20 "$logs/$m.log"
done
skipped=$(cat "$logs"/*.log | grep -c '^\[SKIP\]' || true)
grep -h '^\[SKIP\]' "$logs"/*.log > /dev/null 2>&1 && grep -vE '^\s*(#|$)' "$root/scripts/test/skip.txt" | sed 's/^/   skipped: /'
printf 'test: %d modules, %d failed, %d tests skipped, %ds wall\n' "${#modules[@]}" "${#failed[@]}" "$skipped" "$wall"
[[ ${#failed[@]} -eq 0 ]]
