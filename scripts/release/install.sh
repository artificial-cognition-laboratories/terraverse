#!/bin/sh
# Installs the Terraverse launcher, or updates and repairs an install:
#
#   curl -fsSL https://terraverse.arclabs.it/install.sh | sh
#
#   ~/.terraverse/launcher/Terraverse.AppImage   the launcher; it updates itself after this
#   ~/.terraverse/launcher/terraverse.png        its icon
#   a menu entry, and `terraverse` on your PATH when ~/.local/bin is on it
#
# The launcher installs and runs game versions (into ~/.terraverse/versions).
# Everything else in ~/.terraverse is the player's and is never touched here.
#
# Trusted only if launcher.json verifies against the key below: the key the game
# trusts (scripts/release/update-key.pub; gate.sh checks they match), which CI
# signs with (.github/workflows/launcher.yml). The AppImage and icon must then
# match the hashes it names.
#
# TERRAVERSE_NO_LAUNCH=1 installs without opening it.
set -eu

RELEASES=https://github.com/artificial-cognition-laboratories/terraverse/releases/download/launcher
PUBLIC_KEY='-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEATYuT3HsECn8T2zFIjLdBWF8O7kNFmAchF2nZLnY3EK4=
-----END PUBLIC KEY-----'

fail() { echo "terraverse: $*" >&2; exit 1; }

[ "$(uname -s)-$(uname -m)" = Linux-x86_64 ] || fail "only Linux x86_64 is released so far, this is $(uname -s) $(uname -m)"
for tool in curl openssl sha256sum; do
	command -v "$tool" >/dev/null || fail "$tool is needed: install it and run this again"
done

home=${TERRAVERSE_HOME:-${HOME:?HOME is not set}/.terraverse}
case "$home" in /*) ;; *) fail "TERRAVERSE_HOME must be an absolute path" ;; esac
dir="$home/launcher"
mkdir -p "$dir"
tmp=$(mktemp -d "$dir/.tmp-XXXXXX")
trap 'rm -rf "$tmp"' EXIT

curl -fsSL "$RELEASES/launcher.json" -o "$tmp/launcher.json" || fail "could not download launcher.json: is there a launcher release yet?"
curl -fsSL "$RELEASES/launcher.json.sig" -o "$tmp/launcher.json.sig" || fail "could not download its signature"
printf '%s\n' "$PUBLIC_KEY" > "$tmp/key.pub"
openssl pkeyutl -verify -pubin -inkey "$tmp/key.pub" -rawin -in "$tmp/launcher.json" \
	-sigfile "$tmp/launcher.json.sig" >/dev/null 2>&1 || fail "launcher.json's signature is wrong: not installing it"

# Ours and now verified, so its fixed layout (one field a line, from jq) can be read directly.
field() { sed -n "/\"$1\"/,/}/s/^ *\"$2\": *\"\([^\"]*\)\".*/\1/p" "$tmp/launcher.json"; }
version=$(sed -n 's/^ *"version": *"\([^"]*\)".*/\1/p' "$tmp/launcher.json")
case "$version" in ''|*[!0-9A-Za-z.-]*|.*) fail "launcher.json names a bad version: '$version'" ;; esac

# Fetches a file to $tmp and checks it against its hash in launcher.json.
fetch() {
	url=$(field "$1" url)
	sum=$(field "$1" sha256)
	[ -n "$url" ] && [ -n "$sum" ] || fail "launcher.json has no $1"
	curl -fL --progress-bar "$url" -o "$tmp/$2" || fail "could not download $url"
	echo "$sum  $tmp/$2" | sha256sum -c --status - || fail "the $1 doesn't match launcher.json: not installing it"
}

echo "Downloading the Terraverse launcher $version..."
fetch appimage Terraverse.AppImage
fetch icon terraverse.png
chmod +x "$tmp/Terraverse.AppImage"
# Atomic: a launcher starting at any moment finds the old one or the new one.
mv -f "$tmp/terraverse.png" "$dir/terraverse.png"
mv -f "$tmp/Terraverse.AppImage" "$dir/Terraverse.AppImage"
app="$dir/Terraverse.AppImage"

# An AppImage mounts itself with FUSE 2, which newer distributions leave out.
# Without it, it unpacks itself to run instead: slower to start, but it starts.
run="$app"
if ! { ldconfig -p 2>/dev/null || true; } | grep -q 'libfuse\.so\.2'; then
	run="env APPIMAGE_EXTRACT_AND_RUN=1 $app"
	echo "(no FUSE 2 here: the launcher will unpack itself to start, which takes a moment)"
fi

applications=${XDG_DATA_HOME:-$HOME/.local/share}/applications
mkdir -p "$applications"
cat > "$applications/terraverse.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Terraverse
Comment=An open-source voxel universe
Exec=$run
Icon=$dir/terraverse.png
Terminal=false
Categories=Game;
DESKTOP

echo "The Terraverse launcher $version is installed. Open Terraverse from your applications menu, or run:"
bin="$HOME/.local/bin"
case ":${PATH:-}:" in
	*":$bin:"*)
		printf '#!/bin/sh\nexec %s "$@"\n' "$run" > "$bin/.terraverse.new"
		chmod +x "$bin/.terraverse.new"
		mv -f "$bin/.terraverse.new" "$bin/terraverse"
		echo "  terraverse"
		;;
	*) echo "  $run" ;;
esac

# Open it, unless asked not to or there's no screen to open it on.
if [ "${TERRAVERSE_NO_LAUNCH:-}" != 1 ] && { [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; }; then
	# shellcheck disable=SC2086 # $run is the command and its environment, split on purpose
	nohup $run >/dev/null 2>&1 &
fi
