#!/usr/bin/env bash
# Packs a built tree into a portable Linux folder, then a tarball:
#
#   terraverse-<version>-linux-x86_64/
#     terraverse         the launcher: run this. Player data goes in ~/.terraverse
#     bin/               the engine and Chromium (run in place)
#     lib/               the libraries it needs that a desktop may not have
#     builtin client fonts games/terrafirma locale matter textures ui/dist misc
#     licenses/
#
# Needs a Release build with RUN_IN_PLACE=TRUE and the staged .output tree.
# Usage: scripts/release/package-linux.sh <version> [out-dir]
set -euo pipefail

version=${1:?usage: package-linux.sh <version> [out-dir]}
root=$(cd "$(dirname "$0")/../.." && pwd)
out=${2:-"$root/.output/releases"}
name="terraverse-${version}-linux-x86_64"
stage="$out/$name"

[ -x "$root/.output/bin/luanti" ] || { echo "package: no .output/bin/luanti — build first" >&2; exit 1; }
[ -f "$root/.output/ui/dist/index.html" ] || { echo "package: no .output/ui/dist — cd apps/game/ui && bun run build" >&2; exit 1; }

mkdir -p "$out"
rm -rf "$stage"
mkdir -p "$stage/bin" "$stage/lib" "$stage/games" "$stage/ui" "$stage/licenses"

# The engine and Chromium's runtime. Not what Chromium writes at run time.
for f in "$root"/.output/bin/*; do
	case "$(basename "$f")" in
		minetest|extensions) continue ;;
	esac
	cp -a "$f" "$stage/bin/"
done
strip --strip-unneeded "$stage/bin/luanti"

for d in builtin client fonts locale matter textures misc; do
	cp -a "$root/.output/$d" "$stage/"
done
cp -a "$root/.output/games/terrafirma" "$stage/games/"
cp -a "$root/.output/ui/dist" "$stage/ui/"

cp "$root/LICENSE.txt" "$stage/licenses/luanti-LICENSE.txt"
cp "$root/assets/CREDITS.md" "$stage/licenses/assets-CREDITS.md"
cef=$(ls -d "$root"/.output/build/_deps/cef_binary_*/ | head -1)
cp "$cef/LICENSE.txt" "$stage/licenses/cef-LICENSE.txt"

# Bundle every shared library the engine pulls in, except a desktop's own:
# the C runtime, graphics drivers, the window system, audio, fonts and the
# libraries Chromium expects of the system (GLib, NSS, ATK, CUPS, D-Bus).
# Those must match the machine they run on; everything else must match us.
# Whole names, anchored at .so: libc must not catch libcurl.
system='^(ld-linux-x86-64|linux-vdso|libc|libm|libdl|libpthread|librt|libresolv|libutil|libgcc_s|libstdc\+\+|libatomic'
system+='|libGL[A-Za-z]*|libOpenGL|libEGL|libgbm|libdrm|libvulkan'
system+='|libX[A-Za-z]*|libxcb[-a-z]*|libxkbcommon|libwayland-[a-z]+|libasound|libpulse|libdbus-1|libudev|libsystemd|libcap'
system+='|libfontconfig|libfreetype|libexpat|libz|libglib-2\.0|libgobject-2\.0|libgio-2\.0|libgmodule-2\.0|libffi|libpcre2-8|libmount|libblkid|libselinux'
system+='|libnss3|libnssutil3|libnspr4|libsmime3|libplc4|libplds4|libatk-1\.0|libatk-bridge-2\.0|libatspi|libcups|libavahi-[a-z]+'
# Not libnettle, libhogweed or libunistring: their sonames move between distros
# (Arch has nettle.so.9 where we link .so.8), and each version installs side by side.
system+='|libgnutls|libp11-kit|libtasn1'
system+='|libcairo|libpango-1\.0|libharfbuzz|libfribidi|libthai|libdatrie|libpixman-1|libgraphite2|libpng16'
system+='|libkrb5|libk5crypto|libgssapi_krb5|libcom_err|libkrb5support|libkeyutils)\.so'
LD_LIBRARY_PATH="$root/.output/bin" ldd "$root/.output/bin/luanti" | while read -r lib _ path _; do
	[ -n "$path" ] && [ -f "$path" ] || continue
	lib=$(basename "$lib") # the loader is listed by its full path
	[[ "$lib" =~ $system ]] && continue
	[ "$lib" = libcef.so ] && continue # already in bin/
	cp -L "$path" "$stage/lib/$lib"
done

cat > "$stage/terraverse" <<'LAUNCH'
#!/bin/sh
# Terra Firma: run the engine with its own libraries found first.
here=$(dirname "$(readlink -f "$0")")
export TERRAVERSE_HOME="${TERRAVERSE_HOME:-${HOME:?HOME is not set}/.terraverse}"
LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" exec "$here/bin/luanti" "$@"
LAUNCH
chmod +x "$stage/terraverse"
printf '%s\n' "$version" > "$stage/VERSION"

tar -C "$out" -cf - "$name" | zstd -19 -T0 -q -o "$out/$name.tar.zst" -f
echo "package: $out/$name.tar.zst ($(du -h "$out/$name.tar.zst" | cut -f1)), $(ls "$stage/lib" | wc -l) bundled libraries"
