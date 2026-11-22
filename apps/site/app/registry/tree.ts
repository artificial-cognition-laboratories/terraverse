import type { File } from "./types"

/** A folder or a file in a version's source, folders first, each level sorted. */
export type Node =
    | { kind: "folder"; name: string; path: string; children: Node[] }
    | { kind: "file"; name: string; path: string; file: File }

/** A version's files as a tree. Pure. */
export function tree(files: readonly File[]): Node[] {
    const root: Node[] = []
    for (const file of files) {
        const parts = file.path.split("/")
        let level = root
        parts.forEach((name, i) => {
            const path = parts.slice(0, i + 1).join("/")
            if (i === parts.length - 1) {
                level.push({ kind: "file", name: name, path: path, file: file })
                return
            }
            let folder = level.find((n): n is Extract<Node, { kind: "folder" }> => n.kind === "folder" && n.name === name)
            if (!folder) {
                folder = { kind: "folder", name: name, path: path, children: [] }
                level.push(folder)
            }
            level = folder.children
        })
    }
    return sorted(root)
}

/** Shiki's language for a file, by its extension; plain text for the rest. */
export function language(path: string): string {
    const ext = path.slice(path.lastIndexOf(".") + 1)
    return LANGUAGES[ext] ?? "text"
}

const LANGUAGES: Record<string, string> = { ts: "ts", js: "js", json: "json", md: "md", toml: "toml", css: "css" }

function sorted(nodes: Node[]): Node[] {
    return nodes
        .map(n => (n.kind === "folder" ? { ...n, children: sorted(n.children) } : n))
        .sort((a, b) => (a.kind === b.kind ? a.name.localeCompare(b.name) : a.kind === "folder" ? -1 : 1))
}
