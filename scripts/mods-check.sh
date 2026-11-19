#!/usr/bin/env bash
# TypeScript mods, headless (sprint 119): `terra prepare` on scripts/mods-check/server,
# then the engine runs it with its sidecar. The fixture's mods load in order (one
# importing the other), declare blocks, run timers in game time, throw from a handler,
# take a command typed from the console (a Lua probe), and crash the sidecar once; each
# is checked in the server's log. The game's own mods (registry/mods) are prepared and
# type-checked too.
# Runs terra's unit tests and builds it (apps/cli); needs .output/bin/luanti.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

(cd "$root/apps/cli" && bun run gen:check && bun test 2>&1 | tail -1 && bun run build > /dev/null)

cp -r "$root/scripts/mods-check/server/." "$work/"
"$root/.output/bin/terra" prepare "$work"

# the fixture compiles against the types prepare wrote: the declarations match what runs
tsc="$root/apps/cli/node_modules/.bin/tsc"
for frame in tsconfig.json mods/hello/tsconfig.json mods/hello/server/tsconfig.json mods/greeter/tsconfig.json mods/greeter/server/tsconfig.json; do
	"$tsc" -p "$work/$frame"
done

# the game's own mods (registry/mods, run by apps/game/server) compile against theirs too
"$root/.output/bin/terra" prepare "$root/apps/game/server" > /dev/null
"$tsc" -p "$root/apps/game/server/tsconfig.json"
for mod in "$root"/registry/mods/*/mod.config.ts; do
	dir="$(dirname "$mod")"
	"$root/.output/bin/terra" prepare "$dir" > /dev/null
	"$tsc" -p "$dir/tsconfig.json"
	[[ ! -f "$dir/server/tsconfig.json" ]] || "$tsc" -p "$dir/server/tsconfig.json"
done

world="$work/save/worlds/main"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/mods-check/probe" "$world/worldmods/mods_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = singlenode\n[end_of_params]\n' > "$world/map_meta.txt"
port=$((31200 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout -s INT 8 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" \
	--server-folder "$work" --port "$port" > "$work/log.txt" 2>&1 || true

failed=0
expect() {
	if grep -qE -- "$2" "$work/log.txt"; then
		echo "  ok    $1"
	else
		echo "  FAIL  $1"
		failed=1
	fi
}
expect "mods load in dependency order, sharing exports (hello's code reading hello's settings)"  "\[@check/greeter\] CHECK greeter loaded: Hello, check\."
expect "blocks are registered"                           "mods: 2 blocks from"
expect "a mod reads the server's settings and its defaults" "\[@check/hello\] CHECK settings @check/hello Hello loud=true"
expect "each mod's mod is its own, wherever it's called from" "\[@check/greeter\] CHECK greeter is @check/greeter"
expect "timers run in game time"                         "CHECK after 1\.[0-9] ticks=2 players=0"
expect "a throwing handler is reported, with its source" "\[@check/hello\] Error: CHECK a handler that throws"
expect "the trace names the mod's own file"              "hello/server/main\.ts:[0-9]+"
expect "awaits resume in later windows"                  "CHECK waited"
expect "a dead sidecar is noticed"                       "the sidecar stopped: it exited with status 3"
expect "and restarted, with the same content"            "mods: restarted"
# hello's load-time timer fires again in the restarted sidecar, a second after its first window
if [ "$(grep -cE "CHECK after [0-9.]+ ticks=2" "$work/log.txt")" -eq 2 ]; then echo "  ok    load-time timers count from the restart"; else echo "  FAIL  load-time timers count from the restart"; failed=1; fi
expect "it carries on after"                             "CHECK alive after restart"
expect "a command typed reaches its mod, its args parsed" "\[@check/hello\] CHECK command from console: 42 words after it"
expect "a command typed wrong is answered with its usage"  "\[@check/hello\] n is a whole number, not x\. Usage: /check <n> <text>"
expect "commands answer after a restart"                  "\[@check/hello\] CHECK command from console: 2 again"
if grep -q "their content changed" "$work/log.txt"; then echo "  FAIL  a restart declared different content"; failed=1; fi
if pgrep -f "terra runtime $work" > /dev/null; then echo "  FAIL  the sidecar outlived the server"; failed=1; fi

if [ "$failed" -ne 0 ]; then
	echo "mods check failed. The log:"
	grep -E "mods|ERROR" "$work/log.txt" | sed 's/^/    /' | head -40
	exit 1
fi
echo "mods check: ok"
