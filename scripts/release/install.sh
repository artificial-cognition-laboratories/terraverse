#!/bin/sh
# Installs Terraverse into ~/.terraverse, or updates an install:
#
#   curl -fsSL https://terraverse.arclabs.it/install.sh | sh
#
#   bin/terraverse     the launcher, written below
#   versions/<v>/      a release, read-only in use
#   current, previous  which release runs, and the one to go back to
#
# Everything else in ~/.terraverse is the player's and is never touched here.
# A release is trusted only if its manifest verifies against the key below.
set -eu

RELEASES=https://github.com/artificial-cognition-laboratories/terraverse/releases/latest/download
INSTALL_URL=https://terraverse.arclabs.it/install.sh
PUBLIC_KEY='NOT SET: paste scripts/release/update-key.pub here'

fail() { echo "terraverse: $*" >&2; exit 1; }

[ "$(uname -s)-$(uname -m)" = Linux-x86_64 ] || fail "only Linux x86_64 is released so far, this is $(uname -s) $(uname -m)"
case "$PUBLIC_KEY" in *"BEGIN PUBLIC KEY"*) ;; *) fail "this install script has no signing key yet" ;; esac
for tool in curl tar zstd openssl sha256sum; do
	command -v "$tool" >/dev/null || fail "$tool is needed: install it and run this again"
done

home=${TERRAVERSE_HOME:-${HOME:?HOME is not set}/.terraverse}
case "$home" in /*) ;; *) fail "TERRAVERSE_HOME must be an absolute path" ;; esac
mkdir -p "$home/bin" "$home/versions"
tmp=$(mktemp -d "$home/versions/.tmp-XXXXXX")
trap 'rm -rf "$tmp"' EXIT

curl -fsSL "$RELEASES/manifest.json" -o "$tmp/manifest.json" || fail "could not download the manifest"
curl -fsSL "$RELEASES/manifest.json.sig" -o "$tmp/manifest.json.sig" || fail "could not download its signature"
printf '%s\n' "$PUBLIC_KEY" > "$tmp/key.pub"
openssl pkeyutl -verify -pubin -inkey "$tmp/key.pub" -rawin -in "$tmp/manifest.json" \
	-sigfile "$tmp/manifest.json.sig" >/dev/null 2>&1 || fail "the manifest's signature is wrong: not installing it"

# The manifest is ours and now verified, so its fixed layout can be read directly.
field() { sed -n "/\"linux-x86_64\"/,/}/s/^ *\"$1\": *\"\{0,1\}\([^\",]*\).*/\1/p" "$tmp/manifest.json"; }
version=$(sed -n 's/^ *"version": *"\([^"]*\)".*/\1/p' "$tmp/manifest.json")
url=$(field url)
sum=$(field sha256)
case "$version" in ''|*[!0-9A-Za-z.-]*|.*) fail "the manifest names a bad version: '$version'" ;; esac
[ -n "$url" ] && [ -n "$sum" ] || fail "the manifest has no build for this system"

if [ -e "$home/versions/$version" ]; then
	echo "Terraverse $version is already installed"
else
	echo "Downloading Terraverse $version..."
	curl -fL --progress-bar "$url" -o "$tmp/release.tar.zst" || fail "could not download $url"
	echo "$sum  $tmp/release.tar.zst" | sha256sum -c --status - || fail "the download doesn't match the manifest: not installing it"
	mkdir "$tmp/unpacked"
	tar --zstd -xf "$tmp/release.tar.zst" -C "$tmp/unpacked" --strip-components=1
	[ -x "$tmp/unpacked/terraverse" ] || fail "the release has no launcher in it"
	mv "$tmp/unpacked" "$home/versions/$version"
fi

# Atomic: a launcher started at any moment sees one version or the other.
point() { ln -s "$2" "$tmp/link"; mv -T "$tmp/link" "$home/$1"; }
now=$(readlink "$home/current" 2>/dev/null || true)
if [ "$now" != "versions/$version" ]; then
	[ -z "$now" ] || point previous "$now"
	point current "versions/$version"
fi

# Only the two versions you can run or go back to are kept.
keep=$(readlink "$home/previous" 2>/dev/null || true)
for dir in "$home"/versions/*; do
	case "versions/${dir##*/}" in "versions/$version"|"$keep") ;; *) rm -rf "$dir" ;; esac
done

cat > "$home/bin/.terraverse.new" <<LAUNCHER
#!/bin/sh
# The Terraverse launcher, written by install.sh. It stays this small so that it
# still works when a release doesn't.
set -eu
home=\$(dirname "\$(dirname "\$(readlink -f "\$0")")")
export TERRAVERSE_HOME="\$home"
case "\${1:-}" in
	update) curl -fsSL $INSTALL_URL | sh ;;
	path) echo "\$home" ;;
	versions)
		echo "current:  \$(basename "\$(readlink "\$home/current")")"
		[ ! -L "\$home/previous" ] || echo "previous: \$(basename "\$(readlink "\$home/previous")")"
		;;
	rollback)
		[ -L "\$home/previous" ] || { echo "terraverse: no previous version to go back to" >&2; exit 1; }
		now=\$(readlink "\$home/current")
		back=\$(readlink "\$home/previous")
		ln -s "\$back" "\$home/.link" && mv -T "\$home/.link" "\$home/current"
		ln -s "\$now" "\$home/.link" && mv -T "\$home/.link" "\$home/previous"
		echo "now on \$(basename "\$back")"
		;;
	*) exec "\$home/current/terraverse" "\$@" ;;
esac
LAUNCHER
chmod +x "$home/bin/.terraverse.new"
mv -f "$home/bin/.terraverse.new" "$home/bin/terraverse"

applications=${XDG_DATA_HOME:-$HOME/.local/share}/applications
mkdir -p "$applications"
cat > "$applications/terraverse.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Terraverse
Comment=An open-source voxel universe
Exec="$home/bin/terraverse"
Terminal=false
Categories=Game;
DESKTOP

echo "Terraverse $version is installed. Open it from your applications menu, or run:"
case ":${PATH:-}:" in
	*":$HOME/.local/bin:"*)
		ln -sf "$home/bin/terraverse" "$HOME/.local/bin/terraverse"
		echo "  terraverse"
		;;
	*) echo "  $home/bin/terraverse" ;;
esac
