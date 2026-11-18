import { parse } from "@babel/parser"
import { readFileSync, writeFileSync } from "node:fs"
import { err } from "../../errors"

/** What a config can be given: plain data, as a config holds. */
export type ConfigValue = string | number | boolean | null | readonly ConfigValue[] | { readonly [key: string]: ConfigValue }

type Span = { start: number; end: number }
type Node = Span & { type: string; [key: string]: unknown }
type Property = Node & { key: Node; value: Node }

/**
 * ConfigEditor — changes a project's config file by machine, as its author would by hand.
 *
 * The `defineX({ … })` object is found by parsing, and an edit splices only the bytes that
 * change: re-printing from a syntax tree would reformat the author's code and drop their
 * comments. What it can't place (no define call, a key whose value isn't an object to go
 * into) is refused, never guessed.
 */
export function ConfigEditor(opts: { path: string; define: string }) {
    /** Set the value at `keys`, making the objects on the way. Whether the file changed. */
    function set(keys: readonly string[], value: ConfigValue): { changed: boolean } {
        if (keys.length === 0) throw err("ARGUMENT_INVALID", { detail: "an edit names at least one key" })
        const text = read()
        let target = root(text)
        for (let i = 0; i < keys.length; i++) {
            const key = keys[i]!
            const rest = keys.slice(i + 1)
            const found = ast.property(target, key)
            if (!found) return write(text, layout.insert(text, target, key, literal.nest(rest, value)))
            if (rest.length === 0) return write(text, layout.splice(text, found.value, literal.print(value, layout.indentAt(text, found.start))))
            if (found.value.type !== "ObjectExpression") return write(text, layout.splice(text, found.value, literal.print(literal.nest(rest, value), layout.indentAt(text, found.start))))
            target = found.value
        }
        throw err("UNEXPECTED", { detail: "an edit ran past its keys" })
    }

    /** Remove the property at `keys`, if it's there. Whether the file changed. */
    function remove(keys: readonly string[]): { changed: boolean } {
        const text = read()
        let target = root(text)
        for (let i = 0; i < keys.length; i++) {
            const found = ast.property(target, keys[i]!)
            if (!found) return { changed: false }
            if (i === keys.length - 1) return write(text, layout.cut(text, target, found))
            if (found.value.type !== "ObjectExpression") return { changed: false }
            target = found.value
        }
        return { changed: false }
    }

    function read(): string {
        return readFileSync(opts.path, "utf8")
    }

    /** The object passed to the define call. */
    function root(text: string): Node {
        const found = ast.find(ast.parse(text, opts.path), node =>
            node.type === "CallExpression" && ast.isNode(node.callee) && node.callee.type === "Identifier" && node.callee.name === opts.define)
        const argument = found && Array.isArray(found.arguments) ? found.arguments[0] : undefined
        if (!ast.isNode(argument) || argument.type !== "ObjectExpression")
            throw err("CONFIG_INVALID", { detail: `${opts.path} has no ${opts.define}({ … }) to edit` })
        return argument
    }

    function write(before: string, after: string): { changed: boolean } {
        if (after === before) return { changed: false }
        ast.parse(after, opts.path)   // an edit that breaks the file is a bug here: never write it
        writeFileSync(opts.path, after)
        return { changed: true }
    }

    return { path: opts.path, set: set, remove: remove }
}

export type ConfigEditorT = ReturnType<typeof ConfigEditor>

const INDENT = "    "

/** Reading the config's syntax. */
const ast = {
    parse(text: string, path: string): Node {
        try {
            const tree: unknown = parse(text, { sourceType: "module", plugins: ["typescript"] })
            if (!ast.isNode(tree)) throw new Error("no syntax tree")
            return tree
        } catch (e) {
            throw err("CONFIG_INVALID", { detail: `${path} can't be parsed`, cause: e })
        }
    },

    isNode(value: unknown): value is Node {
        return typeof value === "object" && value !== null && typeof Reflect.get(value, "type") === "string"
            && typeof Reflect.get(value, "start") === "number" && typeof Reflect.get(value, "end") === "number"
    },

    /** The first node, depth first, that `test` accepts. */
    find(node: Node, test: (node: Node) => boolean): Node | null {
        if (test(node)) return node
        for (const [key, child] of Object.entries(node)) {
            if (key === "loc" || key === "comments" || key === "leadingComments" || key === "trailingComments") continue
            for (const each of Array.isArray(child) ? child : [child]) {
                if (!ast.isNode(each)) continue
                const hit = ast.find(each, test)
                if (hit) return hit
            }
        }
        return null
    },

    /** An object's property named `key`, written as `key:` or `"key":`. */
    property(object: Node, key: string): Property | null {
        for (const p of ast.properties(object)) {
            if (p.type !== "ObjectProperty") continue
            const k: unknown = Reflect.get(p, "key")
            const v: unknown = Reflect.get(p, "value")
            if (!ast.isNode(k) || !ast.isNode(v)) continue
            const name = k.type === "Identifier" ? k.name : k.type === "StringLiteral" ? k.value : undefined
            if (name === key) return { ...p, key: k, value: v }
        }
        return null
    },

    properties(object: Node): Node[] {
        return (Array.isArray(object.properties) ? object.properties : []).filter(ast.isNode)
    },
}

/** Placing text in the author's layout. */
const layout = {
    splice(text: string, span: Span, replacement: string): string {
        return text.slice(0, span.start) + replacement + text.slice(span.end)
    },

    /** The whitespace a line starts with, for the line `at` is on. */
    indentAt(text: string, at: number): string {
        const lineStart = text.lastIndexOf("\n", at - 1) + 1
        return /^[ \t]*/.exec(text.slice(lineStart))?.[0] ?? ""
    },

    /** `key: value` added to `object`, in the shape it's already written in. */
    insert(text: string, object: Node, key: string, value: ConfigValue): string {
        const properties = ast.properties(object)
        const last = properties[properties.length - 1]
        if (!last) {
            const outer = layout.indentAt(text, object.start)
            return layout.splice(text, object, `{\n${outer}${INDENT}${literal.key(key)}: ${literal.print(value, outer + INDENT)},\n${outer}}`)
        }
        const indent = layout.indentAt(text, last.start)
        const entry = `${literal.key(key)}: ${literal.print(value, indent)}`
        const sameLine = !text.slice(object.start, last.start).includes("\n")
        if (sameLine) return layout.splice(text, { start: last.end, end: last.end }, `, ${entry}`)
        const comma = text.slice(last.end, object.end - 1).indexOf(",")
        if (comma >= 0) {
            const at = last.end + comma + 1
            return layout.splice(text, { start: at, end: at }, `\n${indent}${entry},`)
        }
        return layout.splice(text, { start: last.end, end: last.end }, `,\n${indent}${entry}`)
    },

    /** `property` taken out of `object`, with its line if it had one to itself, and its comma. */
    cut(text: string, object: Node, property: Property): string {
        let start = property.start
        let end = property.end
        const after = /^\s*,/.exec(text.slice(end))
        if (after) end += after[0].length
        const lineStart = text.lastIndexOf("\n", start - 1) + 1
        if (/^[ \t]*$/.test(text.slice(lineStart, start))) {
            const lineEnd = text.indexOf("\n", end)
            if (/^[ \t]*$/.test(text.slice(end, lineEnd < 0 ? text.length : lineEnd))) {
                start = lineStart
                end = lineEnd < 0 ? text.length : lineEnd + 1
            }
        } else if (!after) {
            // the last on a line of several, with no comma after: take the one before it
            const before = /,\s*$/.exec(text.slice(object.start, start))
            if (before) start -= before[0].length
        }
        return text.slice(0, start) + text.slice(end)
    },
}

/** Values as TypeScript source. */
const literal = {
    key(key: string): string {
        return /^[A-Za-z_$][\w$]*$/.test(key) ? key : JSON.stringify(key)
    },

    /** `{ a: { b: value } }` for keys a, b: what's inserted where a path doesn't exist yet. */
    nest(keys: readonly string[], value: ConfigValue): ConfigValue {
        return keys.reduceRight<ConfigValue>((inner, key) => ({ [key]: inner }), value)
    },

    /** A value as TypeScript, laid out from `indent`. */
    print(value: ConfigValue, indent: string): string {
        if (value === null || typeof value === "number" || typeof value === "boolean") return String(value)
        if (typeof value === "string") return JSON.stringify(value)
        if (literal.isList(value)) {
            if (value.length === 0) return "[]"
            if (value.every(v => v === null || typeof v !== "object")) return `[${value.map(v => literal.print(v, indent)).join(", ")}]`
            return `[\n${value.map(v => `${indent}${INDENT}${literal.print(v, indent + INDENT)},`).join("\n")}\n${indent}]`
        }
        const entries = Object.entries(value)
        if (entries.length === 0) return "{}"
        return `{\n${entries.map(([k, v]) => `${indent}${INDENT}${literal.key(k)}: ${literal.print(v, indent + INDENT)},`).join("\n")}\n${indent}}`
    },

    isList(value: ConfigValue): value is readonly ConfigValue[] {
        return Array.isArray(value)
    },
}
