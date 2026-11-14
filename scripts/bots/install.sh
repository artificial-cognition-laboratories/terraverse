#!/usr/bin/env bash
# Installs the release bot as a systemd user timer: nightly at 03:00, and at
# the next boot if the machine was off. Re-run after changing the schedule.
#   bash scripts/bots/install.sh            install and start the timer
#   bash scripts/bots/install.sh --remove   stop and remove it
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
units="$HOME/.config/systemd/user"
name=terraverse-release

if [[ "${1:-}" == "--remove" ]]; then
    systemctl --user disable --now "$name.timer" 2>/dev/null || true
    rm -f "$units/$name.service" "$units/$name.timer"
    systemctl --user daemon-reload
    echo "removed $name"
    exit 0
fi

mkdir -p "$units"
# A login shell, so mise's tools (claude, gh, bun) and arc are on PATH.
cat >"$units/$name.service" <<UNIT
[Unit]
Description=Terraverse nightly release bot

[Service]
Type=oneshot
WorkingDirectory=$repo
ExecStart=$SHELL -lc '$repo/scripts/bots/release.sh'
TimeoutStartSec=6h
UNIT

cat >"$units/$name.timer" <<UNIT
[Unit]
Description=Run the Terraverse release bot nightly

[Timer]
OnCalendar=*-*-* 03:00
Persistent=true
RandomizedDelaySec=5min

[Install]
WantedBy=timers.target
UNIT

systemctl --user daemon-reload
systemctl --user enable --now "$name.timer"
systemctl --user list-timers "$name.timer" --no-pager
[[ "$(loginctl show-user "$USER" -p Linger --value)" == yes ]] \
    || echo "note: lingering is off, so the timer only fires while you're logged in (loginctl enable-linger $USER)"
