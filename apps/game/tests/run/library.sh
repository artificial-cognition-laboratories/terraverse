#!/usr/bin/env bash
# The pure libraries' tests (each library's tests/, as library/world/terragen/tests): builds only their small
# binaries, never the engine, then runs them side by side through ctest.
#   bun test:hot [library]    the quick cases: the loop to keep running while you work
#   bun test:slow [library]   the slow ones (goldens, Earth-sized worlds)
#   bash apps/game/tests/run/library.sh all [library]
# The first run after a change to a world model's sources works its planets
# out again (they're kept by the sources' hash); later runs read them.
set -euo pipefail
root="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
tier="${1:?usage: test-library.sh hot|slow|all [library]}"
library="${2:-}"
cpus="$(getconf _NPROCESSORS_ONLN)"
case "$tier" in
	hot|slow) labels=(-L "^$tier\$") ;;
	all) labels=() ;;
	*) echo "test-library: no tier '$tier' (hot, slow or all)" >&2; exit 1 ;;
esac
if [[ -n "$library" ]]; then
	target="${library}_tests"
	labels+=(-L "^$library\$")
else
	target=all_library_tests
fi
bash "$root/apps/game/scripts/build.sh" --target "$target" -j"$cpus" > /dev/null
cd "$root/.output/build"
exec ctest "${labels[@]}" -j"$cpus" --output-on-failure --no-tests=error
