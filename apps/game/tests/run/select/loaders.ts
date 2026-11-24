// Which sources load a data file. The build records which headers a source
// includes, never which files it reads at run time, so a changed data file is
// traced through the strings that name it: a source holding "palette.json",
// or the folder it scans ("matter/"), is what reads it. Nothing naming it
// (a name built at run time) is unknown, and the caller runs wide.
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";

export function Loaders(opts: { root: string }) {
	let corpus: { path: string; strings: string[] }[] | null = null;
	const sources = () => {
		corpus ??= execFileSync("git", ["-C", opts.root, "ls-files", "apps/game/src", "apps/game/library", "apps/game/tests"], { encoding: "utf8" })
			.split("\n")
			.filter((path) => /\.(h|hpp|cpp|cc|c|inl)$/.test(path))
			.map((path) => ({ path: path, strings: literals(readFileSync(join(opts.root, path), "utf8")) }));
		return corpus;
	};

	// The sources naming the file, else those naming the folder holding it
	// (a loader that scans it); empty when none do.
	const of = (path: string) => {
		const naming = (names: string[]) =>
			sources()
				.filter(({ strings }) => strings.some((s) => names.some((name) => endsWith(s, name))))
				.map(({ path }) => path);
		const folder = basename(dirname(path));
		const byName = naming([basename(path)]);
		return byName.length > 0 ? byName : naming([folder, `${folder}/`]);
	};
	return { of: of };
}
export type Loaders = ReturnType<typeof Loaders>;

// String literals, one line each (no escapes needed: paths don't carry quotes).
function literals(text: string) {
	return [...text.matchAll(/"([^"\\\n]{2,})"/g)].map((m) => m[1] ?? "");
}

// The literal ends with the name as a whole path part.
function endsWith(literal: string, name: string) {
	return literal === name || literal.endsWith(`/${name}`);
}
