#!/usr/bin/env bash
# Reads a compiler log and lists its errors, grouped by file, and any symbols the link
# couldn't find, so a red build says what broke in a few lines instead of thousands. In
# CI it also writes them to the job summary.
#   cmake --build .output/build -- -k 0 2>&1 | tee build.log; apps/game/scripts/release/build-errors.sh build.log
# Exits 0: it reports; the build's own status decides the step.
set -euo pipefail
log="$1"
# clang/gcc: "path:line:col: error: ...", MSVC/clang-cl: "path(line,col): error ...", the
# linker's own errors (not its warnings).
errors="$(grep -E '(: (fatal )?error:|\): (fatal )?error |^ld: (error|symbol)|^Undefined symbols|undefined reference to)' "$log" | sort -u || true)"
count="$(printf '%s' "$errors" | grep -c . || true)"
# The path up to the line number; a Windows path keeps its drive ("D:\a\...").
files="$(printf '%s\n' "$errors" | sed -nE 's#^(([A-Za-z]:)?[^:( ]+)[:(].*#\1#p' | sed "s#^$PWD/##" | sort | uniq -c | sort -rn || true)"
# ld64 lists each missing symbol quoted and indented under "Undefined symbols"; GNU ld says
# "undefined reference to `sym'".
symbols="$( { sed -nE 's/^[[:space:]]+"([^"]+)", referenced from:.*/\1/p' "$log"; sed -nE "s/.*undefined reference to \`([^']+)'.*/\1/p" "$log"; } | sort -u || true)"

report="$(printf '### Build errors: %s\n\nBy file:\n```\n%s\n```\n\nFirst 60:\n```\n%s\n```\n' \
	"$count" "$files" "$(printf '%s\n' "$errors" | sed "s#$PWD/##g" | head -60)")"
if [[ -n "$symbols" ]]; then
	report+="$(printf '\n\n### Undefined symbols: %s\n```\n%s\n```\n' "$(printf '%s\n' "$symbols" | grep -c .)" "$(printf '%s\n' "$symbols" | head -80)")"
fi
echo "$report"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" && "$count" != 0 ]]; then
	echo "$report" >>"$GITHUB_STEP_SUMMARY"
fi
