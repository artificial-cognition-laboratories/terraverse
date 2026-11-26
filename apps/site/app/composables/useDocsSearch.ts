/**
 * On a phone the docs' search row is hidden until it's asked for (the icon on the where-you-are
 * bar); this is whether it's showing. Above tablet the row is always there and this stays false.
 */
export function useDocsSearch() {
    return useState("docs-searching", () => false)
}
