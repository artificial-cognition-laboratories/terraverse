#!/usr/bin/env bash
# The UI held to its budgets (apps/game/client/shell/ui_check.h): the real
# client on the menu, its page swept by the mouse and scrolled for 8 s, the
# frames judged: the main thread's UI time, input to screen, the page's
# frame gaps. Needs a display and a GPU (a window opens for ~15 s), so it's
# run here, not in CI. Its own user folder: a game already open is no matter.
#   bun check:ui
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
bash "$root/apps/game/scripts/build.sh" -j"$(nproc)" > /dev/null
(cd "$root/apps/game/src/client/app/shell/interface" && bun run build > /dev/null)
bash "$root/apps/game/scripts/stage-runtime.sh" > /dev/null
home="$(mktemp -d)"
trap 'rm -rf "$home"' EXIT
log="$home/client.txt"
TERRAVERSE_HOME="$home" timeout 90 "$root/.output/bin/luanti" --ui-check > "$log" 2>&1 || true
grep -oE 'UICHECK .*' "$log" || true
if grep -qx 'UICHECK pass' <(grep -oE 'UICHECK .*' "$log"); then
	echo "ui check: ok"
else
	echo "ui check: FAILED"
	grep -E "ERROR\[|shell: .*(copied through the CPU|took)" "$log" | head -20 || true
	exit 1
fi
