#!/usr/bin/env bash
# The registry, end to end (sprint 120): terra against the ArcLabs backend's local staging.
# Signs in by the device flow (approved as staging's seeded test user), makes a mod with a
# block and a README image, publishes it, and is refused what it should be: the same version
# again, someone else's scope, a mod.config.ts restating package.json, publishing signed out.
# Then a new server installs the mod from the registry through bun, and the engine boots it
# headless with the mod loaded. Deprecation, a missing mod and the README media are checked
# on the way.
#
# Needs the staging daemon (in ../arclabs: `arc u`) and .output/bin/luanti. terra runs from
# source. Every run publishes a new @testuser/kilns<n> to staging: versions are never deleted.
set -euo pipefail

root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
export ARCLABS_URL="${ARCLABS_URL:-http://localhost:3099}"
# staging's seeded test user (arclabs apps/backend/supabase/seed.sql): approves the sign-in
approver="${REGISTRY_CHECK_KEY:-axon_test_key_0000000000000000000000000000001}"

curl -sf "$ARCLABS_URL/api/health" > /dev/null || { echo "registry check: nothing answering at $ARCLABS_URL. Start staging in ../arclabs with: arc u"; exit 1; }
[ -x "$root/.output/bin/luanti" ] || { echo "registry check: build the engine first (bun run build)"; exit 1; }

work="$(mktemp -d)"
failed=0
trap '[ "$failed" -eq 0 ] && rm -rf "$work" || echo "kept: $work"' EXIT
# terra's sign-in for this run only: never the key of whoever runs the check
export XDG_CONFIG_HOME="$work/config"
export TERRA_ENGINE="$root/.output/bin"
terra() { bun "$root/apps/cli/bin/terra.ts" "$@"; }

ok() { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; [ -z "${2:-}" ] || sed 's/^/        /' <<< "$2" | head -12; failed=1; }
# step <name> <expected exit> <pattern> -- command…: its output must match the pattern
step() {
	local label="$1" want="$2" pattern="$3"; shift 4
	local out got=0
	out="$("$@" 2>&1)" || got=$?
	if [ "$got" -eq "$want" ] && grep -qE -- "$pattern" <<< "$out"; then ok "$label"; else fail "$label (exit $got)" "$out"; fi
}

cd "$work"

# ── signing in ──────────────────────────────────────────────────────────────
terra login > login.txt 2>&1 &
login=$!
code=""
for _ in $(seq 40); do code="$(grep -oE '[A-Z0-9]{3}-[A-Z0-9]{3}' login.txt | head -1 || true)"; [ -n "$code" ] && break; sleep 0.25; done
[ -n "$code" ] || { fail "a sign-in code is shown" "$(cat login.txt)"; exit 1; }
curl -sf -X POST "$ARCLABS_URL/api/user/device/approve" -H "authorization: Bearer $approver" \
	-H 'content-type: application/json' -d "{\"user_code\":\"$code\"}" > /dev/null
wait "$login" && grep -q "Signed in as @testuser" login.txt && ok "login, approved in the browser's place" || fail "login" "$(cat login.txt)"
step "whoami says who"                        0 "@testuser"                          -- terra whoami
[ "$(stat -c %a "$XDG_CONFIG_HOME/terra/account-localhost_3099.json" 2>/dev/null || stat -f %Lp "$XDG_CONFIG_HOME/terra/account-localhost_3099.json")" = "600" ] \
	&& ok "the key is the owner's alone" || fail "the key is the owner's alone"

# ── a mod, published ────────────────────────────────────────────────────────
name="kilns$RANDOM$RANDOM"
mod="@testuser/$name"
terra mod init "$name" --scope @testuser > /dev/null
cd "$name"
cp "$root/apps/game/tests/checks/mods/fixture/mods/hello/assets/textures/brick.png" assets/textures/
cat >> server/main.ts <<EOF
content.blocks.create("$name:brick", { matter: "granite", texture: "brick.png" })
console.log("CHECK $name loaded from the registry")
EOF
mkdir docs && cp assets/textures/brick.png docs/shot.png
printf '# Kilns\n\n![a kiln](docs/shot.png)\n' > README.md
echo "SECRET=1" > .env

step "a dry run lists what would go"          0 "assets/textures/brick.png"          -- terra publish --dry-run
step "but never .env"                         0 "^"                                  -- bash -c "! bun '$root/apps/cli/bin/terra.ts' publish --dry-run --json | grep -q '\.env'"
step "README media travels on its own"        0 '"media":\[\{"path":"docs/shot.png"' -- terra publish --dry-run --json
step "publish"                                0 "Published $mod@0.1.0"               -- terra publish
step "the same version is refused"            1 "already published"                  -- terra publish

cp mod.config.ts mod.config.bak
printf 'export default defineMod({\n    title: "Kilns",\n    version: "0.2.0",\n})\n' > mod.config.ts
step "a mod.config.ts restating package.json" 1 "sets version"                       -- terra publish
mv mod.config.bak mod.config.ts
cp package.json package.bak
sed -i.tmp "s|@testuser/$name|@otheruser/$name|" package.json
sed -i.tmp '0,/"version": "0.1.0"/s//"version": "0.1.1"/' package.json
step "someone else's scope"                   1 "cannot publish to @otheruser"       -- terra publish
mv package.bak package.json

step "info lists it"                          0 "0\.1\.0.*mod API 1"                 -- terra info "$mod"
step "its README image is served"             0 "^200 image/png$"                    -- curl -s -o /dev/null -w "%{http_code} %{content_type}" "$ARCLABS_URL/api/terrafirma/registry/assets/$mod/0.1.0/docs/shot.png"

searched() { curl -s "$ARCLABS_URL/api/terrafirma/registry/search?q=$name"; }
step "it's found by search"                   0 "\"$mod\""                           -- searched
step "deprecate"                              0 "is deprecated"                      -- terra deprecate "$mod" use something else
step "which hides it from search"             0 "^"                                  -- bash -c "! curl -s '$ARCLABS_URL/api/terrafirma/registry/search?q=$name' | grep -q '$mod'"
step "and undo"                               0 "no longer deprecated"               -- terra deprecate "$mod" --undo

# ── a server, installing it ─────────────────────────────────────────────────
cd "$work"
terra server init world > /dev/null
cd world
cp package.json package.before
step "a mod nobody published is refused"      1 "MOD_NOT_PUBLISHED"                  -- terra install "@testuser/nope$RANDOM"
step "leaving package.json as it was"         0 "^"                                  -- cmp package.json package.before
step "install"                                0 "$mod@0\.1\.0"                       -- terra install "$mod"
step "pinned in package.json"                 0 "\"$mod\": \"0\.1\.0\""              -- cat package.json
step "its scope routed to the registry"       0 "\"testuser\" = \{ url = \"$ARCLABS_URL/api/terrafirma/registry/npm/-\" \}" -- cat bunfig.toml

save="$work/world/save/worlds/main"
mkdir -p "$save"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\n' > "$save/world.mt"
printf 'mg_name = singlenode\n[end_of_params]\n' > "$save/map_meta.txt"
port=$((32200 + RANDOM % 500))
timeout -s INT 8 "$root/.output/bin/luanti" --server --config /dev/null --world "$save" \
	--server-folder "$work/world" --port "$port" > "$work/log.txt" 2>&1 || true
step "the engine loads the installed mod"     0 "\[$mod\] CHECK $name loaded from the registry" -- cat "$work/log.txt"
step "with its block"                         0 "mods: 1 blocks from"                -- cat "$work/log.txt"

# ── signed out ──────────────────────────────────────────────────────────────
step "logout"                                 0 "Signed out"                         -- terra logout
cd "$work/$name"
step "publishing signed out is refused"       1 "SIGNED_OUT"                         -- terra publish

if [ "$failed" -ne 0 ]; then echo "registry check failed"; exit 1; fi
echo "registry check: ok ($mod on $ARCLABS_URL)"
