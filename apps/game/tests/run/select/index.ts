// Which suites a change can break. A changed source or header runs the suites
// compiled against its unit (units.ts): its library, or its folder. What the
// build doesn't record (data, build files, the runners themselves, a folder
// not built yet) runs every suite it could reach, so selection errs wide.
//
// It doesn't follow calls across units: a module that reaches the far view
// only through the client's game loop isn't picked for a change to far/. That's
// the fast loop's trade (`--changed`); the full tier and the release gate run
// every suite.
import type { Suite, Suites } from "./suites";
import type { Units } from "./units";

export type Picked = {
	suites: Suite[];
	// the UI's sources changed: its typecheck runs
	ui: boolean;
	// compiled code that no suite is built against: a gap in coverage
	uncovered: string[];
	// one changed path per picked suite, to say why it runs
	why: Map<string, string>;
};

// A changed path, and for a build file the lines that changed (null when the
// change is a whole system or folder asked for by name).
export type Change = { path: string; lines: string[] | null };

type Effect = "none" | "ui" | "engine" | "everything" | "libraries" | "build" | "code" | "other";

// The first row a path matches decides what it can break. Order matters.
const rules: { test: RegExp; effect: Effect }[] = [
	{ test: /^registry\/worlds\//, effect: "none" },
	{ test: /^apps\/game\/src\/client\/app\/shell\/interface\//, effect: "ui" },
	// the runners: what they run changes, so everything they run
	{ test: /^apps\/game\/tests\/run\/select\//, effect: "none" },
	{ test: /^apps\/game\/tests\/run\/library\.sh$/, effect: "libraries" },
	{ test: /^apps\/game\/tests\/run\//, effect: "engine" },
	{ test: /^(CMakeLists\.txt|cmake\/|apps\/game\/CMakeLists\.txt$|apps\/game\/.*\.cmake$)/, effect: "everything" },
	// a folder's build file sets how everything beneath it compiles
	{ test: /^apps\/game\/.+\/CMakeLists\.txt$/, effect: "build" },
	// read by the engine's tests at run time from the staged share directory
	{ test: /^(apps\/game\/)?(assets|game)\/|^registry\/(shaders|textures)\//, effect: "engine" },
	{ test: /^apps\/game\/(src|library|tests)\/.*\.(h|hpp|hh|cpp|cc|c|inl|ipp)$/, effect: "code" },
	{ test: /\.md$/, effect: "none" },
	// the check:* harnesses and benchmarks aren't unit suites
	{ test: /^apps\/game\/tests\/(checks|benchmark)\//, effect: "none" },
	{ test: /^apps\/game\/(src|library|tests)\//, effect: "other" },
	// the site, terra, the launcher, mods, plans: no unit suite runs them
	{ test: /./, effect: "none" },
];

export function Selection(opts: { suites: Suites; units: Units }) {
	const { all, objects } = opts.suites.read();
	const engine = all.filter((suite) => suite.kind === "engine");
	const built = new Set<string>();
	for (const [source, deps] of objects) for (const file of [source, ...deps]) built.add(opts.units.key(file));
	for (const suite of all) for (const unit of suite.reach) built.add(unit);
	const reaching = (unit: string) => all.filter((suite) => suite.reach.has(unit));

	// What each effect runs for one path.
	const effects: Record<Effect, (path: string, lines: string[] | null) => Suite[]> = {
		none: () => [],
		ui: () => [],
		engine: () => engine,
		everything: () => all,
		libraries: () => all.filter((suite) => suite.kind === "library"),
		build: (path, lines) => {
			if (lines !== null && lines.every(listsSources)) return []; // the sources it adds or drops carry their own
			const below = path.slice(0, -"CMakeLists.txt".length);
			return all.filter((suite) => [...suite.reach].some((unit) => unit.startsWith(below)));
		},
		// a unit not in this build (a new folder, another platform's) is unknown, so runs wide
		code: (path) => (built.has(opts.units.key(path)) ? reaching(opts.units.key(path)) : engine),
		// data or a tool beside code: whatever is built against that code
		other: (path) => reaching(opts.units.key(path)),
	};

	const pick = (changes: Change[]): Picked => {
		const why = new Map<string, string>();
		const uncovered: string[] = [];
		let ui = false;
		for (const { path, lines } of changes) {
			const effect = rules.find((rule) => rule.test.test(path))!.effect;
			const suites = effects[effect](path, lines);
			if (effect === "ui") ui = true;
			if (effect === "code" && suites.length === 0) uncovered.push(path);
			for (const suite of suites) if (!why.has(suite.name)) why.set(suite.name, path);
		}
		return { suites: all.filter((suite) => why.has(suite.name)), ui: ui, uncovered: uncovered, why: why };
	};
	return { all: all, pick: pick };
}
export type Selection = ReturnType<typeof Selection>;

// A build file's line that only names sources (`${CMAKE_CURRENT_SOURCE_DIR}/a.cpp`), or a comment.
function listsSources(line: string) {
	const text = line.trim();
	return text === "" || text.startsWith("#") || /^([\w${}./+-]+\.(cpp|cc|c|h|hpp)\s*)+\)?$/.test(text);
}
