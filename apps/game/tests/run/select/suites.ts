// Every unit suite there is, and what each one is built from: the engine's test
// modules (one process each, apps/game/tests/run/engine.sh) and each pure
// library's tests (one ctest label, apps/game/tests/run/library.sh). What a
// suite is built from is read from the last build's depfiles, never listed by
// hand, so a new include is followed the next time the engine builds.
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { Depfiles, type Objects } from "./depfiles";
import type { Units } from "./units";

export type Suite = {
	kind: "engine" | "library";
	name: string;
	// what a run of it is called: the module, or the library's ctest entries
	runs: string[];
	// the units (units.ts) of its sources and every header they include
	reach: Set<string>;
};

export function Suites(opts: { root: string; build: string; units: Units }) {
	const engineObjects = Depfiles({ root: opts.root, build: opts.build, target: "terraverse" });

	const read = () => {
		const objects = engineObjects.read();
		return { objects: objects, all: [...engine(opts.root, objects, opts.units), ...libraries(opts.root, opts.build, opts.units)] };
	};
	return { read: read };
}
export type Suites = ReturnType<typeof Suites>;

// A module is named by its getName(); Catch2's TEST_CASEs all run as one suite.
function engine(root: string, objects: Objects, units: Units): Suite[] {
	const byName = new Map<string, Set<string>>();
	for (const [source, deps] of objects) {
		if (!source.startsWith("apps/game/tests/")) continue;
		const text = readFileSync(join(root, source), "utf8");
		const names = [...text.matchAll(/getName\(\)[^{]*\{\s*return\s+"(\w+)"/g)].map(captured);
		if (/\bTEST_CASE\s*\(/.test(text)) names.push("Catch");
		for (const name of names) {
			const reach = byName.get(name) ?? new Set<string>();
			for (const file of [source, ...deps]) reach.add(units.key(file));
			byName.set(name, reach);
		}
	}
	return [...byName].map(([name, reach]) => ({ kind: "engine", name: name, runs: [name], reach: reach }));
}

// Libraries declare their tests with library_test(<name> …) in their own CMakeLists.
function libraries(root: string, build: string, units: Units): Suite[] {
	const lists = execFileSync("git", ["-C", root, "ls-files", "apps/game/library/**/CMakeLists.txt"], { encoding: "utf8" });
	const suites: Suite[] = [];
	for (const list of lists.split("\n").filter((l) => l.length > 0)) {
		const found = readFileSync(join(root, list), "utf8").match(/^\s*library_test\((\w+)/m);
		if (found === null) continue;
		const name = captured(found);
		const objects = Depfiles({ root: root, build: build, target: `${name}_tests` }).read();
		const reach = new Set<string>();
		for (const [source, deps] of objects) for (const file of [source, ...deps]) reach.add(units.key(file));
		suites.push({ kind: "library", name: name, runs: entries(build, name), reach: reach });
	}
	return suites;
}

function entries(build: string, name: string) {
	const out = execFileSync("ctest", ["--test-dir", build, "-N", "-L", `^${name}$`], { encoding: "utf8" });
	return [...out.matchAll(/Test\s+#\d+:\s+(\S+)/g)].map(captured);
}

function captured(match: RegExpMatchArray) {
	const group = match[1];
	if (group === undefined) throw new Error(`select: no group in '${match[0]}'`);
	return group;
}
