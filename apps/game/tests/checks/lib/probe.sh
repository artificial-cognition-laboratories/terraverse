# The shared shape of a probe check, sourced by tests/checks/*.sh: build the engine, make a
# throwaway world, run one headless server on it with `--probe <name>`, and read the probe's
# tagged lines. A check script says only what world it needs and what its probe must print.
#
#   source "$(dirname "$0")/lib/probe.sh"
#   probe_build                                   # or: probe_build background (nice, 3 jobs)
#   probe_world terra "$probe_atlas_default"      # or: probe_world singlenode; extra map_meta lines after
#   probe_run atlas-world 300 'ATLAS .*'          # writes $work/server.txt and $work/log.txt
#   probe_zero_failed "atlas world" 'ATLAS pass'  # the line's 5th field is the failure count
#
# Sets: root, work (removed on exit), world.

root="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"

# The game's default size, 16384 round (radius 8192 / 2π), and a 250k planet far bigger than
# the map holds at once.
probe_atlas_default="atlas-v2 radius=2607.5945876176133 segments=24 capLatitude=85 core=651.8986469044033 rounding=0.95 height=256"
probe_atlas_250k="atlas-v2 radius=250000 segments=24 capLatitude=85 core=62500 rounding=0.95 height=256"

probe_build() {
	[ -z "${BUILT:-}" ] || return 0 # the caller built it (tests/run/checks.sh)
	if [ "${1:-}" = background ]; then
		nice -n 19 ionice -c 3 bash "$root/apps/game/scripts/build.sh" -j 3 > /dev/null
	else
		bash "$root/apps/game/scripts/build.sh" -j > /dev/null
	fi
}

# probe_world singlenode | probe_world terra <atlas> [extra map_meta line...]
probe_world() {
	work="$(mktemp -d)"
	trap 'rm -rf "$work"' EXIT
	world="$work/world"
	mkdir -p "$world"
	printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\n' > "$world/world.mt"
	case "$1" in
	singlenode) printf 'mg_name = singlenode\nseed = 1\n' > "$world/map_meta.txt"; shift ;;
	terra)
		printf 'mg_name = terra\nmgterra_code = TF24.dGVycmE\nseed = 1\nchunksize = 1\n' > "$world/map_meta.txt"
		local atlas="$2"; shift 2
		local extra; for extra in "$@"; do printf '%s\n' "$extra" >> "$world/map_meta.txt"; done
		printf 'planet_atlas = %s\n' "$atlas" >> "$world/map_meta.txt"
		;;
	*) echo "probe_world: unknown world '$1'" >&2; exit 2 ;;
	esac
	printf '[end_of_params]\n' >> "$world/map_meta.txt"
}

# probe_run <probe> <timeout s> <grep -E pattern of its lines>. The server's exit is not the
# verdict (a probe stops it); the lines are.
probe_run() {
	local port="${PROBE_PORT:-$((30700 + RANDOM % 500))}" # checks.sh gives each its own
	# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
	timeout "$2" "$root/.output/bin/terraverse" --server --config /dev/null --world "$world" --probe "$1" --port "$port" 2>&1 \
		| tee "$work/server.txt" | grep -oE "$3" | tee "$work/log.txt" || true
}

# The server's errors, when a probe printed no verdict.
probe_said() {
	{
		echo "$1 check: ${2:-no result}; the server said:"
		grep -E "ERROR|error" "$work/server.txt" | head -20
	} >&2
	exit 1
}

# probe_line <label> <regex>: print the log's lines matching, or fail with the server's errors.
probe_line() {
	local line; line="$(grep -E "^$2" "$work/log.txt" || true)"
	[ -n "$line" ] || probe_said "$1"
	echo "$line"
}

# probe_zero_failed <label> <regex>: the verdict line's 5th field (its failure count) is 0.
probe_zero_failed() {
	local line; line="$(probe_line "$1" "$2")" || exit 1
	local fail; fail="$(echo "$line" | awk '{print $5}')"
	[ "$fail" = 0 ] && echo "$1 check: ok" || { echo "$1 check: FAILED"; exit 1; }
}
