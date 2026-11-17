#!/usr/bin/env bash
# A nightly owner bot: runs the `owner` skill (.agents/skills/owner) for one
# area, headless in this repo.   scripts/bots/owner.sh testing
# Started by the systemd timers that install.sh sets up; safe to run by hand.
# Every bot shares one lock, so only one works in the checkout at a time; this
# one waits up to 2h for its turn, then skips the night.
set -euo pipefail

area="${1:?usage: owner.sh <area>}"
repo="$(cd "$(dirname "$0")/../.." && pwd)"
[[ -f "$repo/.agents/owners/$area/charter.md" ]] || { echo "owner bot: no charter for '$area'" >&2; exit 1; }
logs="$repo/.output/bots"
day="$(date +%F)"
log="$logs/owner-$area-$day.log"
mkdir -p "$logs"
cd "$repo"

exec 9>"$logs/release.lock"
flock -w 7200 9 || { echo "owner bot ($area): another bot held the lock for 2h" | tee -a "$log"; exit 0; }

notify() { notify-send -a "Terraverse $area owner" "$1" "$2" 2>/dev/null || true; }

echo "== owner $area $(date -Is) at $(git rev-parse --short HEAD)" >>"$log"
status=0
claude -p "/owner $area" \
    --dangerously-skip-permissions \
    --output-format text \
    >>"$log" 2>&1 || status=$?

if [[ $status -eq 0 ]]; then
    notify "$area owner finished" "Report: $logs/owner-$area-$day.md"
else
    notify "$area owner failed ($status)" "Log: $log"
fi
exit "$status"
