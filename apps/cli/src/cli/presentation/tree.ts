import { readdirSync } from "node:fs"
import { join } from "node:path"
import type { TreeNode } from "@arcforge/arcline"

/** What a new project's tree leaves out: what prepare writes, and plumbing nobody edits. */
const HIDDEN = new Set(["node_modules", "bun.lock", "bun.lockb", "tsconfig.json"])

/** Tree — a folder read as a short tree, for init's result. */
export function Tree() {
    /**
 * A folder as a short tree, for init's result: what's on disk, so it's honest. Dotfiles
 * (.gitignore, .terra/) are left out, and so is a folder left empty by that; depth is
 * capped, because it's a summary of the shape, not a file manager.
 */
function read(root: string, depth = 2): TreeNode[] {
    if (depth <= 0) return []
    const nodes: TreeNode[] = []
    for (const entry of readdirSync(root, { withFileTypes: true })) {
        if (entry.name.startsWith(".") || HIDDEN.has(entry.name)) continue
        if (!entry.isDirectory()) {
            nodes.push({ label: entry.name })
            continue
        }
        const children = read(join(root, entry.name), depth - 1)
        if (children.length > 0) nodes.push({ label: entry.name, children: children })
    }
    return nodes
}

    return { read: read }
}

export type TreeT = ReturnType<typeof Tree>
