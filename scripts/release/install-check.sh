#!/usr/bin/env bash
# Runs install.sh against fake signed releases in a temporary home: a first
# install, an update, rollback, pruning, and refusing tampered downloads.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export HOME="$work/user" TERRAVERSE_HOME="$work/user/.terraverse" PATH="$PATH"
unset XDG_DATA_HOME
mkdir -p "$HOME" "$work/site"

openssl genpkey -algorithm ed25519 -out "$work/key" 2>/dev/null
openssl pkey -in "$work/key" -pubout -out "$work/key.pub"

# install.sh pointed at the fake site and trusting the throwaway key
python3 - "$root/scripts/release/install.sh" "$work/key.pub" "$work/site" "$work/install.sh" <<'PY'
import sys
src, pub, site, out = sys.argv[1:]
s = open(src).read()
s = s.replace("PUBLIC_KEY='NOT SET: paste scripts/release/update-key.pub here'", "PUBLIC_KEY='%s'" % open(pub).read().strip())
s = s.replace("RELEASES=https://github.com/artificial-cognition-laboratories/terraverse/releases/latest/download", "RELEASES=file://" + site)
assert "file://" in s and "BEGIN PUBLIC KEY" in s
open(out, "w").write(s)
PY

publish() {
	local v=$1 name="terraverse-v$1-linux-x86_64" dir="$work/pack/terraverse-v$1-linux-x86_64"
	mkdir -p "$dir"
	printf '#!/bin/sh\necho "game %s home=$TERRAVERSE_HOME args=$*"\n' "$v" > "$dir/terraverse"
	chmod +x "$dir/terraverse"
	tar -C "$work/pack" --zstd -cf "$work/site/$name.tar.zst" "$name"
	cat > "$work/site/manifest.json" <<JSON
{
  "version": "$v",
  "builds": {
    "linux-x86_64": {
      "url": "file://$work/site/$name.tar.zst",
      "sha256": "$(sha256sum "$work/site/$name.tar.zst" | cut -d' ' -f1)",
      "size": 1
    }
  }
}
JSON
	openssl pkeyutl -sign -inkey "$work/key" -rawin -in "$work/site/manifest.json" -out "$work/site/manifest.json.sig"
}
expect() { [ "$2" = "$3" ] || { echo "install-check: $1: expected '$3', got '$2'" >&2; exit 1; }; echo "ok: $1"; }
refused() { if sh "$work/install.sh" >"$work/out" 2>&1; then echo "install-check: $1: was installed" >&2; exit 1; fi; grep -q "$2" "$work/out" || { cat "$work/out"; exit 1; }; echo "ok: $1"; }
tv="$TERRAVERSE_HOME/bin/terraverse"

mkdir -p "$TERRAVERSE_HOME/worlds/mine"
publish 1.0.0
sh "$work/install.sh" >/dev/null
expect "first install runs" "$("$tv" --go)" "game 1.0.0 home=$TERRAVERSE_HOME args=--go"
sh "$work/install.sh" >"$work/out" 2>&1
grep -q 'already installed' "$work/out" || { cat "$work/out"; exit 1; }
echo "ok: running it again changes nothing"

publish 1.1.0
sh "$work/install.sh" >/dev/null
expect "update runs the new version" "$("$tv")" "game 1.1.0 home=$TERRAVERSE_HOME args="
"$tv" rollback >/dev/null
expect "rollback runs the old version" "$("$tv")" "game 1.0.0 home=$TERRAVERSE_HOME args="
"$tv" rollback >/dev/null
expect "rolling back again returns" "$("$tv")" "game 1.1.0 home=$TERRAVERSE_HOME args="

publish 1.2.0
sh "$work/install.sh" >/dev/null
expect "only current and previous are kept" "$(cd "$TERRAVERSE_HOME/versions" && ls -A | sort | tr '\n' ' ')" "1.1.0 1.2.0 "
[ -d "$TERRAVERSE_HOME/worlds/mine" ] || { echo "install-check: the player's worlds are gone" >&2; exit 1; }
echo "ok: the player's worlds survive"
grep -q "^Exec=\"$tv\"" "$HOME/.local/share/applications/terraverse.desktop" || { echo "install-check: bad desktop entry" >&2; exit 1; }
echo "ok: desktop entry"

publish 1.3.0
sed -i 's/1\.3\.0/6.6.6/' "$work/site/manifest.json"
refused "a changed manifest is refused" "signature is wrong"
publish 1.3.0
echo tampered >> "$work/site/terraverse-v1.3.0-linux-x86_64.tar.zst"
refused "a changed download is refused" "doesn't match the manifest"
expect "a refused update leaves the game alone" "$("$tv")" "game 1.2.0 home=$TERRAVERSE_HOME args="
expect "a refused update leaves nothing behind" "$(cd "$TERRAVERSE_HOME/versions" && ls -A | sort | tr '\n' ' ')" "1.1.0 1.2.0 "
echo "install-check: ok"
