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
stage apps/game/assets/game games/terrafirma
stage registry/shaders client/shaders
stage registry/textures textures
stage apps/game/assets/fonts fonts
stage apps/game/assets/matter matter
stage apps/game/assets/world world
stage apps/game/assets/misc misc

# TEMP(2026-11-20): apps/game/server (the official mods' server folder) was removed in the
# monorepo move and has no new home yet. Until it does, the server folder staged before
# the move (.output/server) is left as it was; a fresh checkout runs worlds without the
# official mods (Server: a world given no server folder runs the share's, if prepared).
if [[ ! -f "$out/server/.terra/manifest.json" ]]; then
    echo "stage: no official mods staged (apps/game/server was removed; see TEMP above)" >&2
fi

# CMake writes translated .mo files here when gettext is available. Keep
# bundled translations as a fallback without deleting generated output.
if [[ -d "$root/apps/game/assets/locale" ]]; then
    mkdir -p "$out/locale"
    cp -a "$root/apps/game/assets/locale/." "$out/locale/"
fi
