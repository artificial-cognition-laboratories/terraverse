/** Where a project's generated output lives: one owner for the .terra/ layout. */
export type FrameArea = "build" | "fresh" | "logs" | "manifest" | "types"

const AREAS: Record<FrameArea, string> = {
    build: "build",               // mods' halves, bundled
    fresh: "fresh",               // a save made new every run, under terra dev --fresh
    logs: "logs",                 // what ran, kept for reading after
    manifest: "manifest.json",    // what the sidecar loads
    types: "types",               // the declarations a project is checked against
}

/** Frame — a project's .terra/: regenerable, never edited, never committed. */
export function Frame(opts: { root: string }) {
    const dir = `${opts.root}/.terra`

    function path(area: FrameArea): string {
        return `${dir}/${AREAS[area]}`
    }

    return { dir: dir, path: path }
}

export type FrameT = ReturnType<typeof Frame>
