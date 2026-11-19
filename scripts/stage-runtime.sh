#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/.output"

stage() {
    local source=$1 target=$2
    mkdir -p "$out/$target"
    if command -v rsync >/dev/null; then
        rsync -a --delete "$root/$source/" "$out/$target/"
    else # Git Bash on Windows has no rsync
        rm -rf "${out:?}/$target"
        cp -a "$root/$source" "$out/$target"
    fi
}

# The executable uses its parent directory as both share and user path in
# RUN_IN_PLACE builds. Mirror only resources here; user worlds and account
# data in .output are deliberately outside these targets.
stage apps/game/src/builtin builtin
stage apps/game/game games/terrafirma
stage registry/shaders client/shaders
stage registry/textures textures
stage assets/fonts fonts
stage assets/matter matter
stage assets/world world
stage assets/misc misc

# The game's own server folder (apps/game/server): its official TypeScript mods, prepared
# from source here, then only what runs (.terra's build and manifest, which point nowhere
# outside it) staged as the engine's default server folder.
[[ -d "$root/apps/cli/node_modules" ]] || (cd "$root/apps/cli" && bun install --frozen-lockfile >/dev/null)
bun "$root/apps/cli/bin/terra.ts" prepare "$root/apps/game/server" >/dev/null
rm -rf "$out/server"
mkdir -p "$out/server/.terra"
cp "$root/apps/game/server/package.json" "$root/apps/game/server/server.config.ts" "$out/server/"
cp -a "$root/apps/game/server/.terra/build" "$root/apps/game/server/.terra/manifest.json" "$out/server/.terra/"

# CMake writes translated .mo files here when gettext is available. Keep
# bundled translations as a fallback without deleting generated output.
if [[ -d "$root/assets/locale" ]]; then
    mkdir -p "$out/locale"
    cp -a "$root/assets/locale/." "$out/locale/"
fi
