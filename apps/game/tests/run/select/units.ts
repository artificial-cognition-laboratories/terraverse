// The grain selection works at: a changed file breaks whatever is built against
// its unit. A pure library (a folder under apps/game/library with its own
// CMakeLists) is one unit, since its callers see it whole; elsewhere a unit is
// one folder, so a change to walk.cpp reaches a test that includes mover.h
// beside it. A test's own files are units alone: every module includes test.h,
// and a change to one test runs that one. A library's tools/ go with it.
import { execFileSync } from "node:child_process";
import { dirname } from "node:path";

export function Units(opts: { root: string }) {
	const lists = execFileSync("git", ["-C", opts.root, "ls-files", "apps/game/library/**/CMakeLists.txt"], { encoding: "utf8" });
	const libraries = lists
		.split("\n")
		.filter((l) => l.length > 0)
		.map((l) => `${dirname(l)}/`)
		.filter((dir) => !libraryOf(dir, lists)) // a library's own subfolders aren't libraries
		.sort((a, b) => b.length - a.length);

	const key = (path: string) => {
		if (path.startsWith("apps/game/tests/")) return stem(path);
		const library = libraries.find((dir) => path.startsWith(dir));
		// a library's tools/ (render.cpp, built by hand against it) are its own
		return library ?? `${dirname(path)}/`.replace(/^(apps\/game\/library\/.+\/)tools\/$/, "$1");
	};
	return { key: key, libraries: libraries };
}
export type Units = ReturnType<typeof Units>;

export const stem = (path: string) => path.replace(/\.(h|hpp|hh|cpp|cc|c|inl|ipp)$/, "");

// True when an ancestor of dir also has a CMakeLists (vendored trees nest them).
function libraryOf(dir: string, lists: string) {
	let up = dirname(dir.slice(0, -1));
	while (up.startsWith("apps/game/library/")) {
		if (lists.includes(`${up}/CMakeLists.txt\n`)) return true;
		up = dirname(up);
	}
	return false;
}
