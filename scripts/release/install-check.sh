#!/usr/bin/env bash
# Runs install.sh against a fake signed launcher release in a temporary home:
# a first install, a repair, an update, and refusing what wasn't signed or
# doesn't match. The "AppImage" is a script that says which version it is.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export HOME="$work/user" TERRAVERSE_HOME="$work/user/.terraverse" TERRAVERSE_NO_LAUNCH=1
export PATH="$HOME/.local/bin:$PATH"
unset XDG_DATA_HOME
mkdir -p "$HOME/.local/bin" "$work/site"

openssl genpkey -algorithm ed25519 -out "$work/key" 2>/dev/null
openssl pkey -in "$work/key" -pubout -out "$work/key.pub"

# install.sh pointed at the fake site and trusting the throwaway key
python3 - "$root/scripts/release/install.sh" "$work/key.pub" "$work/site" "$work/install.sh" <<'PY'
import re, sys
src, pub, site, out = sys.argv[1:]
s = open(src).read()
s, keys = re.subn(r"PUBLIC_KEY='-----BEGIN PUBLIC KEY-----\n[^\n]*\n-----END PUBLIC KEY-----'", "PUBLIC_KEY='%s'" % open(pub).read().strip(), s)
s, sites = re.subn(r"RELEASES=https://\S+", "RELEASES=file://" + site, s)
assert keys == 1 and sites == 1, "install.sh's key or site line moved"
open(out, "w").write(s)
PY

# A launcher release, as .github/workflows/launcher.yml publishes it.
publish() {
	local v=$1
	printf '#!/bin/sh\necho "launcher %s"\n' "$v" > "$work/site/terraverse-launcher-$v-x86_64.AppImage"
	printf 'PNG' > "$work/site/terraverse.png"
	jq -n --arg v "$v" \
		--arg url "file://$work/site/terraverse-launcher-$v-x86_64.AppImage" \
		--arg sum "$(sha256sum "$work/site/terraverse-launcher-$v-x86_64.AppImage" | cut -d' ' -f1)" \
		--arg icon "file://$work/site/terraverse.png" --arg iconsum "$(sha256sum "$work/site/terraverse.png" | cut -d' ' -f1)" \
		'{version: $v, appimage: {url: $url, sha256: $sum}, icon: {url: $icon, sha256: $iconsum}}' > "$work/site/launcher.json"
	openssl pkeyutl -sign -inkey "$work/key" -rawin -in "$work/site/launcher.json" -out "$work/site/launcher.json.sig"
}
expect() { [ "$2" = "$3" ] || { echo "install-check: $1: expected '$3', got '$2'" >&2; exit 1; }; echo "ok: $1"; }
refused() { if sh "$work/install.sh" >"$work/out" 2>&1; then echo "install-check: $1: was installed" >&2; exit 1; fi; grep -q "$2" "$work/out" || { cat "$work/out"; exit 1; }; echo "ok: $1"; }
desktop="$HOME/.local/share/applications/terraverse.desktop"

refused "nothing released yet is said plainly" "is there a launcher release yet"

mkdir -p "$TERRAVERSE_HOME/worlds/mine" "$TERRAVERSE_HOME/versions/0.1.2"
publish 0.1.0
sh "$work/install.sh" >/dev/null 2>&1
expect "the command runs the launcher" "$(terraverse)" "launcher 0.1.0"
grep -q "^Exec=.*/.terraverse/launcher/Terraverse.AppImage" "$desktop" || { cat "$desktop"; exit 1; }
grep -q "^Icon=$TERRAVERSE_HOME/launcher/terraverse.png" "$desktop" || { cat "$desktop"; exit 1; }
echo "ok: a menu entry with its icon"
[ -d "$TERRAVERSE_HOME/worlds/mine" ] && [ -d "$TERRAVERSE_HOME/versions/0.1.2" ] || { echo "install-check: the player's things were touched" >&2; exit 1; }
echo "ok: worlds and game versions are left alone"

rm "$TERRAVERSE_HOME/launcher/Terraverse.AppImage"
sh "$work/install.sh" >/dev/null 2>&1
expect "running it again repairs" "$(terraverse)" "launcher 0.1.0"

publish 0.2.0
sh "$work/install.sh" >/dev/null 2>&1
expect "it updates" "$(terraverse)" "launcher 0.2.0"

cp "$work/site/launcher.json" "$work/launcher.json.good"
sed -i 's/0\.2\.0/6.6.6/' "$work/site/launcher.json"
refused "a changed launcher.json is refused" "signature is wrong"
cp "$work/launcher.json.good" "$work/site/launcher.json"
echo 'evil' >> "$work/site/terraverse-launcher-0.2.0-x86_64.AppImage"
refused "a changed AppImage is refused" "doesn't match launcher.json"
expect "and the installed one is left working" "$(terraverse)" "launcher 0.2.0"
[ -z "$(find "$TERRAVERSE_HOME/launcher" -name '.tmp-*')" ] || { echo "install-check: a refused install left files behind" >&2; exit 1; }
echo "ok: nothing left behind"

echo "install-check: ok"
