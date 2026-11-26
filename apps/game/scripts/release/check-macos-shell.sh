#!/usr/bin/env bash
# macOS: Chromium's framework and helper apps are laid out where the shell looks
# (client/app/shell/host/framework.h), and the client runs with Chromium: its loop, pumped
# from the engine's frames on macOS (host/pump.h), takes the UI check to its verdict.
# The page doesn't paint on macOS yet, so the verdict is fail; this demands it be reached
# (no refusal, crash or hang) and says which. Next: require "UICHECK pass".
# A screenshot of the screen lands in <out dir> if given.
#   apps/game/scripts/release/check-macos-shell.sh <bin dir> [out dir]
# CI runs it on the macOS runner; bash 3.2.
set -euo pipefail

bin="$1"
frameworks="$bin/../Frameworks"
fail() { echo "check-macos-shell: $1" >&2; exit 1; }

lib="$frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
[[ -f "$lib" ]] || fail "no framework at $lib"
for suffix in "" " (Alerts)" " (GPU)" " (Plugin)" " (Renderer)"; do
	name="Terraverse Helper$suffix"
	[[ -x "$frameworks/$name.app/Contents/MacOS/$name" ]] || fail "no helper app '$name'"
	plutil -lint "$frameworks/$name.app/Contents/Info.plist" >/dev/null || fail "'$name' has a bad Info.plist"
done
echo "check-macos-shell: framework and 5 helper apps in place"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export TERRAVERSE_HOME="$work/home"

# The client, until it stops or 60 s pass (no `timeout` on macOS).
"$bin/terraverse" --ui-check >"$work/client.log" 2>&1 &
pid=$!
shot=0
for i in $(seq 1 120); do
	kill -0 "$pid" 2>/dev/null || break
	if [[ $i == 20 && -n "${2:-}" ]]; then
		mkdir -p "$2"
		screencapture -x "$2/macos-shell.png" && shot=1 || true
	fi
	sleep 0.5
done
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
cat "$work/client.log"

[[ $shot == 1 ]] && echo "check-macos-shell: screenshot in $2/macos-shell.png"
# A verdict means the page loaded and answered: Chromium's loop runs.
verdict="$(grep -oE "UICHECK (pass|fail)" "$work/client.log" | tail -1 || true)"
[[ -n "$verdict" ]] || fail "the UI check reached no verdict: the client and Chromium didn't run together (log above)"
echo "check-macos-shell: the client runs with Chromium; the UI check's verdict: ${verdict#UICHECK }"
