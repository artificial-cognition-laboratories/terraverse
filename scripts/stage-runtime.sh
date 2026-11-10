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
stage registry/mods/clientmods clientmods
stage registry/games games
stage apps/game/game games/terrafirma
stage registry/shaders client/shaders
stage registry/textures textures
stage assets/fonts fonts
stage assets/matter matter
stage assets/misc misc

# CMake writes translated .mo files here when gettext is available. Keep
# bundled translations as a fallback without deleting generated output.
if [[ -d "$root/assets/locale" ]]; then
    mkdir -p "$out/locale"
    cp -a "$root/assets/locale/." "$out/locale/"
fi
