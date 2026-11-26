// Which check:* harnesses (apps/game/tests/checks) a change reaches. They boot a real server or
// client, so unit selection can't see them: this table says what each one exercises, by path
// prefix from the repo root. `--changed` names them ("worth running: check:atlas-world"); it
// doesn't run them, since some need a display, the network or minutes.
const game = "apps/game/";
const atlas = [
	`${game}library/world/planet/`,
	`${game}library/world/terragen/`,
	`${game}src/server/world/generate/`,
	`${game}src/server/world/terra.`,
	`${game}tests/probe/atlas.cpp`,
	`${game}tests/probe/world.cpp`,
	`${game}assets/world/`,
];
const probes = [`${game}tests/probe/probe.`, `${game}tests/checks/lib/`];

const table: Record<string, string[]> = {
	"check:terragen": [`${game}library/world/terragen/`, `${game}src/server/world/generate/`],
	"check:land": [`${game}library/world/terragen/`, `${game}src/server/world/generate/`, `${game}src/client/views/game/far/`],
	"check:far": [`${game}src/client/views/game/far/`, `${game}library/world/terragen/`, `${game}assets/game/palette.json`],
	"check:atlas-world": [...atlas, ...probes],
	"check:atlas-land": [...atlas, ...probes],
	"check:atlas-seams": [...atlas, `${game}tests/probe/seams.cpp`, ...probes],
	"check:atlas-debug": [...atlas, ...probes],
	"check:atlas-dig": [...atlas, `${game}src/server/game/`, ...probes],
	"check:window": [...atlas, ...probes],
	"check:bodies": [...atlas, ...probes],
	"check:interact": [`${game}src/server/game/`, `${game}tests/probe/interact.cpp`, ...probes],
	"check:matter": [`${game}library/matter/`, `${game}assets/matter/`, `${game}tests/probe/small.cpp`, ...probes],
	"check:wiki": [`${game}library/matter/sandbox/`, `${game}tests/checks/wiki/`],
	"check:moves": [`${game}library/physics/`, `${game}assets/game/moves.json`, `${game}assets/game/vehicles.json`],
	"check:mods": [`${game}src/server/app/mods/`, "apps/cli/src/runtime/", "registry/mods/", `${game}server/`, `${game}tests/checks/mods/`, "library/types/"],
	"check:dev": ["apps/cli/src/terra/dev/", `${game}src/server/app/mods/`],
	"check:serve": ["apps/cli/src/terra/running/", "apps/cli/src/terra/world/", `${game}src/shared/platform/online/`, `${game}src/server/app/access/`],
	"check:palette": [`${game}assets/game/palette.json`, "registry/textures/", `${game}assets/world/`],
};

export function Checks() {
	// The checks a set of changed paths reaches, each with the first path that reached it.
	const reached = (paths: string[]) => {
		const why = new Map<string, string>();
		for (const path of paths)
			for (const [check, prefixes] of Object.entries(table))
				if (!why.has(check) && (path === ownScript(check) || prefixes.some((prefix) => path.startsWith(prefix))))
					why.set(check, path);
		return why;
	};
	return { reached: reached };
}
export type Checks = ReturnType<typeof Checks>;

// A check's own script: check:atlas-seams is atlas-seam.sh, the rest their name.
function ownScript(check: string) {
	const name = check.slice("check:".length);
	return `${game}tests/checks/${name === "atlas-seams" ? "atlas-seam" : name}.sh`;
}
