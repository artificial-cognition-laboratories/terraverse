#!/usr/bin/env bash
# Installs the nightly bots as systemd user timers, each also run at the next
# boot if the machine was off. In order: the owners (testing 19:00, CI 21:00)
# improve their areas and file jobs, jobs (23:00) works through .agents/jobs,
# release (03:00) ships what changed. They share one lock and wait their turn.
# Re-run after changing a schedule.
#   bash scripts/bots/install.sh            install and start the timers
#   bash scripts/bots/install.sh --remove   stop and remove them
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
units="$HOME/.config/systemd/user"

# unit  time  timeout  script [args]   (timeouts include waiting for the lock)
bots=(
    "owner-testing 19:00 6h owner.sh testing"
    "owner-ci 21:00 6h owner.sh ci"
    "jobs 23:00 6h jobs.sh"
    "release 03:00 10h release.sh"
)

for entry in "${bots[@]}"; do
    read -r bot at timeout script <<<"$entry"
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
ExecStart=$SHELL -lc '$repo/scripts/bots/$script'
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
