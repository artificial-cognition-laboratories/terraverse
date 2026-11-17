#!/usr/bin/env bash
# Installs the nightly bots as systemd user timers, each also run at the next
# boot if the machine was off: jobs at 23:00 (works through .agents/jobs), then
# release at 03:00 (ships what changed, waiting for jobs if it's still going).
# Re-run after changing a schedule.
#   bash scripts/bots/install.sh            install and start the timers
#   bash scripts/bots/install.sh --remove   stop and remove them
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
units="$HOME/.config/systemd/user"

# bot  time  timeout (release includes up to 4h waiting for the jobs bot)
bots=(
    "jobs 23:00 4h"
    "release 03:00 10h"
)

for entry in "${bots[@]}"; do
    read -r bot at timeout <<<"$entry"
    name="terraverse-$bot"

    if [[ "${1:-}" == "--remove" ]]; then
        systemctl --user disable --now "$name.timer" 2>/dev/null || true
        rm -f "$units/$name.service" "$units/$name.timer"
        echo "removed $name"
        continue
    fi

    mkdir -p "$units"
    # A login shell, so mise's tools (claude, gh, bun) and arc are on PATH.
    cat >"$units/$name.service" <<UNIT
[Unit]
Description=Terraverse nightly $bot bot

[Service]
Type=oneshot
WorkingDirectory=$repo
ExecStart=$SHELL -lc '$repo/scripts/bots/$bot.sh'
TimeoutStartSec=$timeout
UNIT

    cat >"$units/$name.timer" <<UNIT
[Unit]
Description=Run the Terraverse $bot bot nightly

[Timer]
OnCalendar=*-*-* $at
Persistent=true
RandomizedDelaySec=5min

[Install]
WantedBy=timers.target
UNIT
done

systemctl --user daemon-reload
[[ "${1:-}" == "--remove" ]] && exit 0

for entry in "${bots[@]}"; do
    read -r bot _ <<<"$entry"
    systemctl --user enable --now "terraverse-$bot.timer"
done
systemctl --user list-timers 'terraverse-*' --no-pager
[[ "$(loginctl show-user "$USER" -p Linger --value)" == yes ]] \
    || echo "note: lingering is off, so the timers only fire while you're logged in (loginctl enable-linger $USER)"
