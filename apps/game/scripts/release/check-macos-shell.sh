#!/usr/bin/env bash
# macOS: Chromium's framework and helper apps are laid out where the shell looks
# (client/app/shell/host/framework.h), and the client runs with Chromium: its loop, pumped
# from the engine's frames on macOS (host/pump.h), loads the page and paints it, and the UI
# check reaches its verdict. The verdict itself isn't held: the runner has no GPU, so the
# planetarium behind the menu draws in software at about a frame a second and no frame budget
# can be met. A screenshot, a few seconds after the page's first frame, lands in <out dir>.
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

# The client, until it stops or 60 s pass (no `timeout` on macOS). Under lldb, so a crash
# leaves its threads' backtraces in the log: the engine's own crash report said nothing on
# macOS (2026-11-27; perhaps Chromium's handlers replace ours, not yet known).
lldb --batch -o run -k "thread backtrace all" -k "kill" -- "$bin/terraverse" --ui-check >"$work/client.log" 2>&1 &
pid=$!
shot=0
painted=0
for i in $(seq 1 120); do
	kill -0 "$pid" 2>/dev/null || break
	if [[ $painted == 0 ]] && grep -q "the page's first frame" "$work/client.log"; then painted=$i; fi
	if [[ $painted != 0 && $i == $((painted + 6)) && -n "${2:-}" ]]; then
		mkdir -p "$2"
		screencapture -x "$2/macos-shell.png" && shot=1 || true
	fi
	sleep 0.5
done
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
cat "$work/client.log"

[[ $shot == 1 ]] && echo "check-macos-shell: screenshot in $2/macos-shell.png"
grep -q "shell: loaded shell://ui/index.html (200)" "$work/client.log" || fail "the page never loaded (log above)"
grep -q "the page's first frame" "$work/client.log" || fail "the page loaded but never painted (log above)"
echo "check-macos-shell: the page loaded and Chromium painted it (whether it reaches the screen: the screenshot)"
# A verdict means the client kept running with Chromium to the end of the check.
verdict="$(grep -oE "UICHECK (pass|fail)" "$work/client.log" | tail -1 || true)"
[[ -n "$verdict" ]] || fail "the UI check reached no verdict: the client and Chromium didn't run together (log above)"
echo "check-macos-shell: the client runs with Chromium; the UI check's verdict: ${verdict#UICHECK }"
