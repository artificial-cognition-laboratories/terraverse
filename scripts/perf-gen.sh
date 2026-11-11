#!/usr/bin/env bash
# Where making a world spends its time (terragen/progress.h), at a few sizes,
# cold (nothing kept) and warm (opened again), as `bun perf:gen`. Each run's a
# line in .output/perf/gen.jsonl, and the last like it is shown beside it.
#   [SIZES="314160 628320"] [CODE=TF22.Y3gyMzlqNHY] bun perf:gen
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
code="${CODE:-TF22.Y3gyMzlqNHY}"
sizes="${SIZES:-314160 628320}"
out="$root/.output/perf"
tool="$out/perf"
log="$out/gen.jsonl"
mkdir -p "$out"
touch "$log"
bash "$root/scripts/terragen-tool.sh" "$tool" "$root/scripts/terragen-check/perf.cpp" 2> >(grep -v DT_TEXTREL >&2)
keep="$(mktemp -d)"
trap 'rm -rf "$keep"' EXIT
for size in $sizes; do
	for warm in false true; do
		last="$(grep -F "\"code\":\"$code@$size\",\"size\":$size,\"warm\":$warm," "$log" | tail -1 || true)"
		echo "== $size blocks round, $([ $warm = true ] && echo warm || echo cold)"
		SIZE="$size" KEEP="$keep" JSON="$log" "$tool" "$code"
		if [ -n "$last" ]; then
			echo "   last time: $(sed -E 's/^.*"warm":[a-z]+,"seconds":([0-9.]+),.*/\1/' <<< "$last") s"
		fi
	done
done
