#!/usr/bin/env bash
# Reads a compiler log and lists its errors, grouped by file, so a red build says
# what broke in a few lines instead of thousands. In CI it also writes them to the
# job summary.
#   cmake --build .output/build -- -k 0 2>&1 | tee build.log; scripts/release/build-errors.sh build.log
# Exits 0: it reports; the build's own status decides the step.
set -euo pipefail
log="$1"
# clang/gcc: "path:line:col: error: ...", MSVC/clang-cl: "path(line,col): error ...", the linker's own lines.
errors="$(grep -E '(: (fatal )?error:|\): (fatal )?error |^ld: |^Undefined symbols|undefined reference to)' "$log" | sort -u || true)"
count="$(printf '%s' "$errors" | grep -c . || true)"
# The path up to the line number; a Windows path keeps its drive ("D:\a\...").
files="$(printf '%s\n' "$errors" | sed -nE 's#^(([A-Za-z]:)?[^:( ]+)[:(].*#\1#p' | sed "s#^$PWD/##" | sort | uniq -c | sort -rn || true)"

report="$(printf '### Build errors: %s\n\nBy file:\n```\n%s\n```\n\nFirst 60:\n```\n%s\n```\n' \
	"$count" "$files" "$(printf '%s\n' "$errors" | sed "s#$PWD/##g" | head -60)")"
echo "$report"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" && "$count" != 0 ]]; then
	echo "$report" >>"$GITHUB_STEP_SUMMARY"
fi
