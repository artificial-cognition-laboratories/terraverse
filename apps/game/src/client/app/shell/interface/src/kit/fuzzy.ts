/**
 * Finding what's typed in a label, as a command palette does: the letters in order, not
 * necessarily together. `marks` are the label's matched indices (to light them); a higher
 * `score` reads more like what was meant (runs of letters, a word's first letter, early).
 * Null: not there.
 */
export function fuzzy(label: string, typed: string): { score: number; marks: number[] } | null {
    const want = typed.trim().toLowerCase()
    if (want === "") return { score: 0, marks: [] }
    const text = label.toLowerCase()
    // as one run first: what's typed whole, at a word's start best
    const at = text.indexOf(want)
    if (at >= 0) {
        const start = at === 0 || /[\s:/>@#._-]/.test(text[at - 1]!)
        return { score: 1000 - at + (start ? 500 : 0), marks: Array.from(want, (_, i) => at + i) }
    }
    const marks: number[] = []
    let score = 0
    let from = 0
    for (const ch of want) {
        if (ch === " ") continue
        const i = text.indexOf(ch, from)
        if (i < 0) return null
        if (marks.length && i === marks[marks.length - 1]! + 1) score += 5
        if (i === 0 || /[\s:/>@#._-]/.test(text[i - 1]!)) score += 8
        score -= i - from
        marks.push(i)
        from = i + 1
    }
    return { score: score, marks: marks }
}
