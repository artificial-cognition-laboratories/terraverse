#!/usr/bin/env bash
# The unit suites: the engine's modules (engine.sh) and the pure libraries' tests
# (library.sh), every one or only what a change reaches (tests/run/select).
#   bun run test                        every suite: the full tier, as the release gate runs it
#   bun run test --changed [base]       what uncommitted changes reach (or everything since base)
#   bun run test <system|path>...       what covers far, physics, library/world/terragen, a file
#   bun run test TestFar Catch          those engine modules
#   --hot                               libraries' quick cases only (bun check --changed passes it)
# Selection reads the last build's depfiles, so it builds first when anything
# compiled might run. It prints why each suite runs; files no suite is built
# against are listed as gaps.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
run="$root/apps/game/tests/run"
cpus="$(getconf _NPROCESSORS_ONLN)"
tier=all
if [[ "${1:-}" == --hot ]]; then tier=hot; shift; fi

if [[ $# -eq 0 ]]; then
	bash "$run/library.sh" "$tier"
	exec bash "$run/engine.sh"
fi

modules=() names=()
for a in "$@"; do
	if [[ "$a" =~ ^(Test[A-Za-z0-9_]+|Catch)$ ]]; then modules+=("$a"); else names+=("$a"); fi
done
if [[ ${#names[@]} -gt 0 ]]; then
	pick() { bun "$run/select/main.ts" "${names[@]}"; }
	# Built first, so the depfiles name what the sources include now (BUILT=1: it is).
	# A first pick that fails (no build yet) builds too; the second says why.
	picked="$(pick 2>/dev/null || echo unknown)"
	if [[ -z "${BUILT:-}" && -n "$picked" && "$picked" != ui ]]; then
		bash "$root/apps/game/scripts/build.sh" -j"$cpus" > /dev/null
		export BUILT=1
	fi
	picked="$(pick)"
	while read -r kind name; do
		case "$kind" in
			engine) modules+=("$name") ;;
			library) bash "$run/library.sh" "$tier" "$name" ;;
			ui) bun run --cwd "$root/apps/game/src/client/app/shell/interface" typecheck ;;
			"") ;;
			*) echo "test: select printed '$kind'" >&2; exit 1 ;;
		esac
	done <<< "$picked"
fi
[[ ${#modules[@]} -eq 0 ]] && { echo "test: no engine module reached"; exit 0; }
exec bash "$run/engine.sh" "${modules[@]}"
