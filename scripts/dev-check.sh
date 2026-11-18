#!/usr/bin/env bash
# terra dev, headless (sprint 119): a new mod, run as it's edited. A code save reloads the
# mods without restarting; a new block restarts the universe; a broken save is reported and
# the last build keeps running; fixing it reloads; Ctrl-C leaves nothing running.
# Needs .output/bin/luanti and .output/bin/terra (bun check:mods builds terra).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
terra="$root/.output/bin/terra"
work="$(mktemp -d)"
trap 'kill -INT "${dev:-0}" 2>/dev/null || true; [ "$failed" -eq 0 ] && rm -rf "$work" || echo "kept: $work"' EXIT

cd "$work"
"$terra" mod init kilns > /dev/null
cd kilns
cp "$root/scripts/mods-check/server/mods/hello/assets/textures/brick.png" assets/textures/
port=$((31900 + RANDOM % 90))
log="$work/dev.txt"
"$terra" dev --headless --port "$port" > "$log" 2>&1 &
dev=$!

# wait until the log has had `n` lines matching `pattern`, up to `s` seconds
await() { for _ in $(seq $(( $3 * 4 ))); do [ "$(grep -cE -- "$2" "$log" || true)" -ge "$1" ] && return 0; sleep 0.25; done; return 1; }

failed=0
step() { if "${@:2}"; then echo "  ok    $1"; else echo "  FAIL  $1"; failed=1; fi; }

step "it starts, with the mod prepared"          await 1 "listening on" 90
echo 'console.log("CHECK edited")' >> server/main.ts
step "a code save reloads the mods"              await 1 "\[kilns\] CHECK edited" 10
step "without restarting the universe"           bash -c "! grep -q 'Restarting the universe' '$log'"
echo 'content.blocks.create("kilns:brick", { matter: "granite", texture: "brick.png" })' >> server/main.ts
step "a new block restarts the universe"         await 1 "Restarting the universe" 10
step "which comes back with it"                  await 1 "mods: 1 blocks from" 60
echo 'this is not typescript {' >> server/main.ts
step "a broken save is reported"                 await 1 "BUILD_FAILED" 10
step "naming the file and line"                  await 1 "server/main.ts:[0-9]+:[0-9]+" 2
sed -i '$d' server/main.ts
step "fixing it reloads"                         await 3 "mods: reloaded" 10
kill -INT "$dev"
step "Ctrl-C stops terra dev"                    bash -c "for _ in \$(seq 40); do kill -0 $dev 2>/dev/null || exit 0; sleep 0.25; done; exit 1"
# [l] and [t]: so pgrep never finds this line's own command
step "and nothing is left running"               bash -c "! pgrep -f '[l]uanti --server.*--port $port' > /dev/null && ! pgrep -f '[t]erra runtime $work' > /dev/null"

if [ "$failed" -ne 0 ]; then
	echo "dev check failed. What terra dev showed:"
	sed "s/^/    /" "$log" | head -80
	exit 1
fi
echo "dev check: ok"
