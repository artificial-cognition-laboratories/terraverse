import type { Element, ElementContent, RootContent } from "hast"
import type { Highlighter } from "@nuxtjs/mdc"
import type { CodeTab } from "~/config/home"

// Code as Shiki's HTML, in the docs' theme so code reads the same everywhere on
// the site. Shiki is loaded on first use: the landing runs it while prerendering,
// so its page carries the HTML, never Shiki itself.

/** One file's text, one block per line. */
export async function highlightCode(code: string, lang: string): Promise<string> {
    const { codeToHtml } = await import("shiki")
    return lines(await codeToHtml(code, { lang: lang, theme: "dark-plus" }))
}

/** Each of the landing's code tabs. */
export async function highlight(tabs: readonly CodeTab[]): Promise<string[]> {
    return Promise.all(tabs.map(t => highlightCode(t.code, t.lang)))
}

// Each line is its own block: the \n Shiki leaves between them would be a second, empty line
function lines(html: string): string {
    return html.replaceAll('</span>\n<span class="line">', '</span><span class="line">')
}

/**
 * Code blocks in markdown (a mod's readme), highlighted while the page is parsed,
 * so a prerendered page carries its colours: MDC's own highlighter answers from a
 * server route that a static site doesn't have. A language Shiki doesn't know is
 * shown as plain text: a stranger's readme can't break the page with a typo.
 */
export const markdownHighlighter: Highlighter = async (code, lang) => {
    const { codeToHast, bundledLanguages } = await import("shiki")
    const root = await codeToHast(code, { lang: lang in bundledLanguages ? lang : "text", theme: "dark-plus" })
    const pre = root.children.find(isElement)
    const inner = pre?.children.find(isElement)
    if (!pre || !inner) throw new Error("Shiki's tree has no <pre><code>")
    return {
        // Each line is its own block, as in lines() above
        tree: inner.children.filter(n => !(n.type === "text" && n.value === "\n")),
        inlineStyle: typeof pre.properties.style === "string" ? pre.properties.style : "",
    }
}

function isElement(node: RootContent | ElementContent): node is Element {
    return node.type === "element"
}
