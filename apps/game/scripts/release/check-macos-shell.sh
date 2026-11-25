#!/usr/bin/env bash
# macOS: Chromium's framework and helper apps are laid out where the shell looks
# (client/app/shell/host/framework.h), and a client start gets as far as the shell
# allows. Today that's the framework loaded and the refusal at Chromium's message loop,
# which isn't wired on macOS; when it is, this expects the UI check to pass instead.
#   apps/game/scripts/release/check-macos-shell.sh <bin dir>
# CI runs it on the macOS runner; bash 3.2.
set -euo pipefail

bin="$1"
frameworks="$bin/../Frameworks"
fail() { echo "check-macos-shell: $1" >&2; exit 1; }

lib="$frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
[[ -f "$lib" ]] || fail "no framework at $lib"
for suffix in "" " (Alerts)" " (GPU)" " (Plugin)" " (Renderer)"; do
	name="Terraverse Helper$suffix"
	[[ -x "$frameworks/$name.app/Contents/MacOS/$name" ]] || fail "no helper app '$name'"
	plutil -lint "$frameworks/$name.app/Contents/Info.plist" >/dev/null || fail "'$name' has a bad Info.plist"
done
echo "check-macos-shell: framework and 5 helper apps in place"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export TERRAVERSE_HOME="$work/home"

# The client, until it stops or 60 s pass (no `timeout` on macOS).
"$bin/terraverse" --ui-check >"$work/client.log" 2>&1 &
pid=$!
for _ in $(seq 1 120); do
	kill -0 "$pid" 2>/dev/null || break
	sleep 0.5
done
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
cat "$work/client.log"

# The framework loaded, or the shell would have refused before this.
grep -qF "shell: not wired on macOS yet (Chromium's message loop)" "$work/client.log" \
	|| fail "the client didn't reach Chromium's message loop (log above)"
echo "check-macos-shell: Chromium's framework loads in the client"
