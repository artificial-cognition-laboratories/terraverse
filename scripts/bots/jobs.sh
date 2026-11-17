#!/usr/bin/env bash
# The nightly jobs bot: runs the `jobs` skill (.agents/skills/jobs) headless in
# this repo, working through .agents/jobs. Started by the systemd timer that
# install.sh sets up; safe to run by hand. It shares the release bot's lock, so
# the two never work in the checkout at once; the release bot, later in the
# night, waits for it and ships what it committed.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
logs="$repo/.output/bots"
day="$(date +%F)"
log="$logs/jobs-$day.log"
mkdir -p "$logs"
cd "$repo"

exec 9>"$logs/release.lock"
flock -n 9 || { echo "jobs bot: a bot is already running" | tee -a "$log"; exit 0; }

notify() { notify-send -a "Terraverse jobs" "$1" "$2" 2>/dev/null || true; }

echo "== jobs bot $(date -Is) at $(git rev-parse --short HEAD)" >>"$log"
status=0
claude -p "/jobs" \
    --dangerously-skip-permissions \
    --output-format text \
    >>"$log" 2>&1 || status=$?

if [[ $status -eq 0 ]]; then
    notify "Jobs bot finished" "Report: $logs/jobs-$day.md"
else
    notify "Jobs bot failed ($status)" "Log: $log"
fi
exit "$status"
