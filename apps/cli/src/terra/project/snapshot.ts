import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs"
import { join } from "node:path"

/**
 * Snapshot — some of a project's files as they are now, to put back if what changes them
 * fails. A file that didn't exist is removed again.
 */
export function Snapshot(opts: { root: string; files: readonly string[] }) {
    const saved = opts.files.map(name => {
        const path = join(opts.root, name)
        return { path: path, text: existsSync(path) ? readFileSync(path, "utf8") : null }
    })

    function restore(): void {
        for (const file of saved) {
            if (file.text === null) rmSync(file.path, { force: true })
            else writeFileSync(file.path, file.text)
        }
    }

    return { restore: restore }
}

export type SnapshotT = ReturnType<typeof Snapshot>
