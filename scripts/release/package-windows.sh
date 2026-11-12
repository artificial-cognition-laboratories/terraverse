#!/usr/bin/env bash
# Packs a built tree into a portable Windows folder, then a tarball:
#
#   terraverse-<version>-windows-x86_64/
#     bin/terraverse.exe   run this. Player data goes in %APPDATA%\Terraverse (main.cpp)
#     bin/                 the engine, Chromium, vcpkg's DLLs and the VC++ runtime
#     builtin client fonts games/terrafirma locale matter textures ui/dist misc
#     licenses/
#
# Needs a Release build with RUN_IN_PLACE=TRUE and the staged .output tree, and
# Visual Studio's environment (VCToolsRedistDir) for the runtime DLLs: a
# fresh Windows install may not have them. Run from Git Bash.
# Usage: scripts/release/package-windows.sh <version> [out-dir]
set -euo pipefail

version=${1:?usage: package-windows.sh <version> [out-dir]}
root=$(cd "$(dirname "$0")/../.." && pwd)
out=${2:-"$root/.output/releases"}
name="terraverse-${version}-windows-x86_64"
stage="$out/$name"

[ -f "$root/.output/bin/luanti.exe" ] || { echo "package: no .output/bin/luanti.exe — build first" >&2; exit 1; }
[ -f "$root/.output/ui/dist/index.html" ] || { echo "package: no .output/ui/dist — cd apps/game/ui && bun run build" >&2; exit 1; }
[ -n "${VCToolsRedistDir:-}" ] || { echo "package: VCToolsRedistDir is unset — run inside Visual Studio's environment" >&2; exit 1; }

mkdir -p "$out"
rm -rf "$stage"
mkdir -p "$stage/bin" "$stage/games" "$stage/ui" "$stage/licenses"

# The engine, Chromium's runtime and the DLLs vcpkg put beside them. Not the
# debug info or import library, nor what Chromium writes at run time.
for f in "$root"/.output/bin/*; do
	case "$(basename "$f")" in
		*.pdb|*.lib|*.exp|*.ilk|extensions) continue ;;
		luanti.exe) cp "$f" "$stage/bin/terraverse.exe" ;; # the launcher runs bin\terraverse.exe (game.rs)
		*) cp -a "$f" "$stage/bin/" ;;
	esac
done

# The C++ runtime the engine and vcpkg link (/MD), next to the executable.
crt=$(cygpath -u "$VCToolsRedistDir")/x64
crt=$(command ls -d "$crt"/Microsoft.VC*.CRT | head -1)
cp "$crt"/*.dll "$stage/bin/"

for d in builtin client fonts locale matter textures misc; do
	cp -a "$root/.output/$d" "$stage/"
done
cp -a "$root/.output/games/terrafirma" "$stage/games/"
cp -a "$root/.output/ui/dist" "$stage/ui/"

cp "$root/LICENSE.txt" "$stage/licenses/luanti-LICENSE.txt"
cp "$root/assets/CREDITS.md" "$stage/licenses/assets-CREDITS.md"
cef=$(command ls -d "$root"/.output/build/_deps/cef_binary_*/ | head -1)
cp "$cef/LICENSE.txt" "$stage/licenses/cef-LICENSE.txt"

printf '%s\n' "$version" > "$stage/VERSION"

tar -C "$out" -cf - "$name" | zstd -19 -T0 -q -o "$out/$name.tar.zst" -f
echo "package: $out/$name.tar.zst ($(du -h "$out/$name.tar.zst" | cut -f1)), $(command ls "$stage/bin"/*.dll | wc -l) DLLs"
