// Flat paths to the rows of a tree, depth first, folders before files. Pure.

export type TreeRow = { path: string; name: string; depth: number; folder: boolean; type: string; size: string }

export function fileTree(files: readonly { path: string; size: number }[]): TreeRow[] {
    type Node = { folders: Map<string, Node>; files: { name: string; path: string; size: number }[] }
    const root: Node = { folders: new Map(), files: [] }
    for (const file of files) {
        const parts = file.path.split("/")
        let node = root
        for (const part of parts.slice(0, -1)) {
            if (!node.folders.has(part)) node.folders.set(part, { folders: new Map(), files: [] })
            node = node.folders.get(part)!
        }
        node.files.push({ name: parts.at(-1) ?? file.path, path: file.path, size: file.size })
    }
    const rows: TreeRow[] = []
    const walk = (node: Node, prefix: string, depth: number) => {
        for (const [name, child] of [...node.folders].sort(([a], [b]) => a.localeCompare(b))) {
            rows.push({ path: prefix + name + "/", name: name, depth: depth, folder: true, type: "", size: "" })
            walk(child, prefix + name + "/", depth + 1)
        }
        for (const file of [...node.files].sort((a, b) => a.name.localeCompare(b.name))) {
            rows.push({ path: file.path, name: file.name, depth: depth, folder: false, type: file.name.split(".").pop() ?? "", size: bytes(file.size) })
        }
    }
    walk(root, "", 0)
    return rows
}

function bytes(n: number): string {
    return n < 1024 ? `${n} B` : n < 1024 * 1024 ? `${(n / 1024).toFixed(1)} kB` : `${(n / 1024 / 1024).toFixed(1)} MB`
}
