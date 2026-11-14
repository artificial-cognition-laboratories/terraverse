#!/usr/bin/env bash
# The nightly release bot: runs the `release` skill (.agents/skills/release)
# headless in this repo. Started by the systemd timer that install.sh sets up;
# safe to run by hand. It works in the main checkout, not a worktree, because
# arc's targets are pinned to this path.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
logs="$repo/.output/bots"
day="$(date +%F)"
log="$logs/release-$day.log"
mkdir -p "$logs"
cd "$repo"

# One bot at a time; a run still going when the next fires is left alone.
exec 9>"$logs/release.lock"
flock -n 9 || { echo "release bot: already running" | tee -a "$log"; exit 0; }

notify() { notify-send -a "Terraverse release" "$1" "$2" 2>/dev/null || true; }

echo "== release bot $(date -Is) at $(git rev-parse --short HEAD)" >>"$log"
status=0
claude -p "/release" \
    --dangerously-skip-permissions \
    --output-format text \
    >>"$log" 2>&1 || status=$?

if [[ $status -eq 0 ]]; then
    notify "Release bot finished" "Report: $logs/release-$day.md"
else
    notify "Release bot failed ($status)" "Log: $log"
fi
exit "$status"
