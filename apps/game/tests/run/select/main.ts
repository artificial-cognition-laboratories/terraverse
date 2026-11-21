// What a change runs. Prints one line per suite to run (`engine TestFar`,
// `library terragen`, `ui`) on stdout for apps/game/tests/run/test.sh, and why
// on stderr.
//   bun tests/run/select/main.ts --changed [base]   uncommitted changes, or everything since base
//   bun tests/run/select/main.ts --commit <rev>     one commit
//   bun tests/run/select/main.ts <system|path>...   what covers far/, physics, a file
//   bun tests/run/select/main.ts --measure [n]      suites per commit over the last n (20)
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { type Change, Selection, type Picked } from "./index";
import { Suites } from "./suites";
import { Units } from "./units";

const root = execFileSync("git", ["rev-parse", "--show-toplevel"], { cwd: import.meta.dir, encoding: "utf8" }).trim();
const git = (...args: string[]) =>
	execFileSync("git", ["-C", root, ...args], { encoding: "utf8" }).split("\n").filter((l) => l.length > 0);

const units = Units({ root: root });
const suites = Suites({ root: root, build: join(root, ".output/build"), units: units });
const selection = Selection({ suites: suites, units: units });

const [verb, ...rest] = process.argv.slice(2);
if (verb === undefined) throw new Error("select: give --changed [base], --commit <rev>, --measure [n] or a system or path");
if (verb === "--measure") measure(Number(rest[0] ?? 20));
else if (verb === "--changed") report(selection.pick(changed(rest[0])));
else if (verb === "--commit") report(selection.pick(commit(rest[0])));
else report(selection.pick([verb, ...rest].flatMap(named)));

function changed(base: string | undefined): Change[] {
	const diffs = [...(base === undefined ? [] : [[`${base}...HEAD`]]), ["HEAD"]];
	const untracked = git("ls-files", "--others", "--exclude-standard").map((path) => ({ path: path, lines: null }));
	return [...diffs.flatMap((range) => described(["diff", ...range])), ...untracked];
}

function commit(rev: string | undefined): Change[] {
	if (rev === undefined) throw new Error("select: --commit needs a revision");
	return described(["diff-tree", "--no-commit-id", "-r", "--root", rev]);
}

// The paths a diff touches; a build file's carry the lines it changed, since
// one that only lists sources is no wider than the sources.
function described(diff: string[]): Change[] {
	return git(...diff, "--name-only").map((path) => ({
		path: path,
		lines: path.endsWith("CMakeLists.txt")
			? git(...diff, "-U0", "--", path).filter((l) => /^[-+](?![-+]{2} )/.test(l)).map((l) => l.slice(1))
			: null,
	}));
}

// A path (file or folder) stands for its files; a bare name for every folder
// of that name in the game's sources (`far`, `physics`, `terragen`).
function named(name: string): Change[] {
	return files(name).map((path) => ({ path: path, lines: null }));
}

function files(name: string) {
	const path = existsSync(name) ? resolve(name) : join(root, name);
	if (existsSync(path)) return git("ls-files", "--full-name", "--", path);
	const folders = git("ls-files", "apps/game/src", "apps/game/library").filter((f) => f.split("/").slice(0, -1).includes(name));
	if (folders.length === 0) throw new Error(`select: no path or folder named '${name}' in apps/game/src or apps/game/library`);
	return folders;
}

function report(picked: Picked) {
	for (const suite of picked.suites) {
		console.log(`${suite.kind} ${suite.name}`);
		console.error(`  ${suite.kind} ${suite.name.padEnd(28)} ${picked.why.get(suite.name)}`);
	}
	if (picked.ui) console.log("ui");
	for (const path of picked.uncovered) console.error(`  no suite is built against ${path}`);
	const runs = picked.suites.reduce((n, s) => n + s.runs.length, 0);
	const total = selection.all.reduce((n, s) => n + s.runs.length, 0);
	console.error(`select: ${runs} of ${total} suites${picked.ui ? ", and the UI's typecheck" : ""}`);
}

// The Scoreboard's "suites run for a typical change": a library counts each of
// its ctest entries, an engine module counts one.
function measure(n: number) {
	const total = selection.all.reduce((sum, s) => sum + s.runs.length, 0);
	const counts = git("log", `-${n}`, "--format=%h %s").map((line) => {
		const [sha] = line.split(" ");
		const runs = selection.pick(commit(sha)).suites.reduce((sum, s) => sum + s.runs.length, 0);
		console.log(`${String(runs).padStart(4)}  ${line.slice(0, 90)}`);
		return runs;
	});
	const sorted = [...counts].sort((a, b) => a - b);
	const at = (i: number) => sorted[i] ?? Number.NaN;
	const mid = Math.floor(sorted.length / 2);
	const median = sorted.length % 2 === 1 ? at(mid) : (at(mid - 1) + at(mid)) / 2;
	console.log(`median ${median} of ${total} suites over ${counts.length} commits`);
}
