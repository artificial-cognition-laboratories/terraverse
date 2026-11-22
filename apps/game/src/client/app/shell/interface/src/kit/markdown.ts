// A README's markdown as plain data, for Markdown.vue to draw. It never produces HTML,
// so a README can't run anything in the page: text stays text whatever it contains.

export type Span = { text: string; code?: boolean; bold?: boolean; em?: boolean; href?: string; image?: string }
export type Block =
    | { kind: "heading"; level: 1 | 2 | 3 | 4; spans: Span[] }
    | { kind: "paragraph"; spans: Span[] }
    | { kind: "list"; ordered: boolean; items: Span[][] }
    | { kind: "code"; lang: string; text: string }
    | { kind: "quote"; spans: Span[] }
    | { kind: "image"; alt: string; src: string }
    | { kind: "table"; head: Span[][]; rows: Span[][][] }
    | { kind: "rule" }

/** Blocks from markdown; `resolve` turns an image's path into where it's served. */
export function parseMarkdown(source: string, resolve: (src: string) => string): Block[] {
    const lines = source.replace(/\r\n?/g, "\n").split("\n")
    const blocks: Block[] = []
    let at = 0
    /** Takes lines while `keep` holds for the next one. */
    const take = (keep: (line: string) => boolean): string[] => {
        const out: string[] = []
        for (let line = lines[at]; line !== undefined && keep(line); line = lines[++at]) out.push(line)
        return out
    }
    for (let line = lines[at]; line !== undefined; line = lines[at]) {
        const fence = /^```\s*(\S*)/.exec(line)
        const heading = /^(#{1,6})\s+(.*)$/.exec(line)
        const image = /^\s*!\[([^\]]*)\]\(([^)\s]+)[^)]*\)\s*$/.exec(line)
        if (fence) {
            at++
            const body = take((l) => !l.startsWith("```"))
            at++
            blocks.push({ kind: "code", lang: fence[1] ?? "", text: body.join("\n") })
        } else if (!line.trim()) {
            at++
        } else if (heading) {
            at++
            blocks.push({ kind: "heading", level: Math.min(4, heading[1]?.length ?? 1) as 1 | 2 | 3 | 4, spans: inline(heading[2] ?? "", resolve) })
        } else if (RULE.test(line)) {
            at++
            blocks.push({ kind: "rule" })
        } else if (image) {
            at++
            blocks.push({ kind: "image", alt: image[1] ?? "", src: resolve(image[2] ?? "") })
        } else if (ROW.test(line) && DIVIDER.test(lines[at + 1] ?? "")) {
            const head = cells(line).map((c) => inline(c, resolve))
            at += 2
            const rows = take((l) => ROW.test(l)).map((l) => cells(l).map((c) => inline(c, resolve)))
            blocks.push({ kind: "table", head: head, rows: rows })
        } else if (line.startsWith(">")) {
            const body = take((l) => l.startsWith(">")).map((l) => l.replace(/^>\s?/, ""))
            blocks.push({ kind: "quote", spans: inline(body.join(" "), resolve) })
        } else if (ITEM.test(line)) {
            const ordered = /^\s*\d/.test(line)
            const items = take((l) => ITEM.test(l)).map((l) => inline(l.replace(ITEM, ""), resolve))
            blocks.push({ kind: "list", ordered: ordered, items: items })
        } else {
            // the first line always belongs (so "#tag" can't stall the loop), then until a block starts
            at++
            const body = [line, ...take((l) => l.trim() !== "" && !/^(#|```|>|\||\s*([-*+]|\d+[.)])\s)/.test(l))]
            blocks.push({ kind: "paragraph", spans: inline(body.map((l) => l.trim()).join(" "), resolve) })
        }
    }
    return blocks
}

const RULE = /^(-{3,}|\*{3,}|_{3,})\s*$/
// a table: rows of pipes, the head's divider of dashes (and colons) under its first
const ROW = /^\s*\|.*\|\s*$/
const DIVIDER = /^\s*\|(\s*:?-+:?\s*\|)+\s*$/

/** A table row's cells, trimmed, without the outer pipes. */
function cells(line: string): string[] {
    return line.trim().replace(/^\|/, "").replace(/\|$/, "").split("|").map((c) => c.trim())
}
const ITEM = /^\s*([-*+]|\d+[.)])\s+/
const INLINE = /`([^`]+)`|!\[([^\]]*)\]\(([^)\s]+)[^)]*\)|\[([^\]]+)\]\(([^)\s]+)[^)]*\)|\*\*([^*]+)\*\*|__([^_]+)__|\*([^*]+)\*|_([^_]+)_/g

function inline(text: string, resolve: (src: string) => string): Span[] {
    const spans: Span[] = []
    let at = 0
    for (const m of text.matchAll(INLINE)) {
        if (m.index > at) spans.push({ text: text.slice(at, m.index) })
        const [, code, alt, src, label, href, bold, bold2, em, em2] = m
        if (code !== undefined) spans.push({ text: code, code: true })
        else if (src !== undefined) spans.push({ text: alt ?? "", image: resolve(src) })
        else if (href !== undefined) spans.push({ text: label ?? "", href: href })
        else if (bold !== undefined || bold2 !== undefined) spans.push({ text: bold ?? bold2 ?? "", bold: true })
        else spans.push({ text: em ?? em2 ?? "", em: true })
        at = m.index + m[0].length
    }
    if (at < text.length) spans.push({ text: text.slice(at) })
    return spans
}
