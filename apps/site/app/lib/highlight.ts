import type { CodeTab } from "~/config/home"

/**
 * Each tab's code as Shiki's HTML, in the docs' theme so code reads the same
 * everywhere on the site. Run while prerendering: the page carries the HTML,
 * never Shiki itself.
 */
export async function highlight(tabs: readonly CodeTab[]): Promise<string[]> {
    const { codeToHtml } = await import("shiki")
    const html = await Promise.all(tabs.map(t => codeToHtml(t.code, { lang: t.lang, theme: "dark-plus" })))
    // Each line is its own block: the \n Shiki leaves between them would be a second, empty line
    return html.map(h => h.replaceAll('</span>\n<span class="line">', '</span><span class="line">'))
}
