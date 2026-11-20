#!/usr/bin/env bash
# The unit tests' time budgets, held: each module that times itself, alone,
# with TF_BUDGETS set (apps/game/tests/unit/cpu_time.h). The parallel unit
# run (bun run test) reports these times but doesn't hold them: with every
# core busy a step costs twice as much, even in CPU time. Run it on a quiet
# machine; it says what the load was, so a failure can be read against it.
#   bun check:budgets
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
bin="$root/.output/bin/luanti"
bash "$root/apps/game/scripts/build.sh" -j"$(nproc)" > /dev/null
echo "budgets: load average $(cut -d' ' -f1-3 /proc/loadavg) on $(nproc) cores"
failed=0
# (ATLAS_BUDGET: TestAtlasRender's costs alone, not its views)
log="$(mktemp)"
trap 'rm -f "$log"' EXIT
for m in TestSandbox TestAtlasRender; do
	if TF_BUDGETS=1 ATLAS_BUDGET=1 "$bin" --test-module "$m" --info > "$log" 2>&1; then r=0; else r=1; fi
	grep -E 'ms a 1 s step|a budget of|choosing|\[FAIL\]|assertion' "$log" | sed 's/^.*INFO\[Main\]: //' || true
	if [[ $r != 0 ]]; then
		echo "== $m over budget"
		failed=1
	fi
done
exit $failed
