// The compiler's own record of what each object was built from: every `.o.d`
// under a target's object folders (`<target>.dir`, wherever CMake put them),
// read into source → the repo files it includes.
// Folders an older layout left behind are skipped by their source no longer
// existing; when two objects claim one source, the newer depfile wins.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative } from "node:path";

export type Objects = Map<string, string[]>;

export function Depfiles(opts: { root: string; build: string; target: string }) {
	const read = (): Objects => {
		const dirs = [...folders(opts.build, `${opts.target}.dir`)];
		if (dirs.length === 0) throw new Error(`select: no objects for ${opts.target} in ${relative(opts.root, opts.build)}: build first`);
		const newest = new Map<string, { at: number; deps: string[] }>();
		for (const file of dirs.flatMap((dir) => [...walk(dir)])) {
			const files = parse(readFileSync(file, "utf8"), opts.root);
			const source = files[0];
			if (source === undefined || !existsSync(join(opts.root, source))) continue;
			const at = statSync(file).mtimeMs;
			const seen = newest.get(source);
			if (seen === undefined || seen.at < at) newest.set(source, { at: at, deps: files.slice(1) });
		}
		return new Map([...newest].map(([source, entry]) => [source, entry.deps]));
	};
	return { read: read };
}
export type Depfiles = ReturnType<typeof Depfiles>;

// `obj.o: src.cpp a.h b.h \` → the repo-relative files after the colon, the
// source first. System headers fall outside the repo and are dropped.
function parse(text: string, root: string) {
	const words = text.replace(/\\\n/g, " ").split(/\s+/).filter((w) => w.length > 0);
	return words.slice(1).filter((w) => w.startsWith(`${root}/`)).map((w) => relative(root, w));
}

function* walk(dir: string): Generator<string> {
	for (const entry of readdirSync(dir, { withFileTypes: true })) {
		const path = join(dir, entry.name);
		if (entry.isDirectory()) yield* walk(path);
		else if (entry.name.endsWith(".o.d")) yield path;
	}
}

function* folders(dir: string, name: string): Generator<string> {
	for (const entry of readdirSync(dir, { withFileTypes: true })) {
		if (!entry.isDirectory() || entry.name === "_deps") continue;
		const path = join(dir, entry.name);
		if (entry.name === name) yield path;
		else yield* folders(path, name);
	}
}
