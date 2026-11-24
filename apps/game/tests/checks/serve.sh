#!/usr/bin/env bash
# Local servers (sprint 125): terra server init, terra host, the running/ record the game
# lists, admission by its secret, and nothing left behind. Joining opens the game's window:
# with no display it's skipped, and says so.
# Needs .output/bin/luanti and .output/bin/terra (bun check:mods builds terra).
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
terra="$root/.output/bin/terra"
engine="$root/.output/bin/luanti"
work="$(mktemp -d)"
export TERRAVERSE_HOME="$work/home" # a home of its own: never the real one's running/
failed=0
trap '[ -n "${host:-}" ] && kill -TERM "$host" 2>/dev/null || true; [ "$failed" -eq 0 ] && rm -rf "$work" || echo "kept: $work"' EXIT

step() { if "${@:2}"; then echo "  ok    $1"; else echo "  FAIL  $1"; failed=1; fi; }
await() { for _ in $(seq $(( $2 * 4 ))); do eval "$1" && return 0; sleep 0.25; done; return 1; }

cd "$work"
"$terra" server init hello > init.txt
step "init makes the server where you are"         test -f hello/server.config.ts
cd hello
port=$((32000 + RANDOM % 90))
record="$TERRAVERSE_HOME/running/$port.json"
"$terra" serve --port "$port" > "$work/host.txt" 2>&1 &
host=$!

step "it starts, and says where to join"            await "grep -q 'terra://localhost:$port' '$work/host.txt' && grep -q 'listening on' '$work/host.txt'" 90
step "its record is written, the user's alone"      await "[ -f '$record' ] && [ \"\$(stat -c %a '$record')\" = 600 ]" 5
step "the record names it by its ref"               test "$(jq -r .ref "$record")" = "terra://localhost:$port"
step "the secret isn't on its command line"         bash -c "! tr '\0' ' ' < /proc/$(jq .pid "$record")/cmdline | grep -q '$(jq -r .secret "$record")'"

if [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
	jq -r .secret "$record" > "$work/secret"
	echo "not-the-secret-not-the-secret-not-the-secret" > "$work/bad"
	timeout 40 "$engine" --address 127.0.0.1 --port "$port" --name visitor --password-file "$work/secret" --go > "$work/good.txt" 2>&1 || true
	timeout 30 "$engine" --address 127.0.0.1 --port "$port" --name intruder --password-file "$work/bad" --go > "$work/bad.txt" 2>&1 || true
	step "its secret lets you in"                    grep -q "visitor .* joins game" "$work/host.txt"
	step "anything else is refused, with why"        grep -q "intruder was refused: This server's run secret didn't match" "$work/host.txt"
	step "a refused client exits cleanly"            bash -c "! grep -q 'fatal error' '$work/bad.txt'"
else
	echo "  skip  joining: no display for the game's window"
fi

kill -TERM "$host"
wait "$host" || true
step "stopped, its record is gone"                  test ! -e "$record"

[ "$failed" -eq 0 ] && echo "serve: ok" || { echo "serve: FAILED"; exit 1; }
