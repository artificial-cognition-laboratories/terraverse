/** What a word of a shell command is, for colouring it as the docs' bash blocks do (dark-plus). */
export type Word = { text: string; kind: "program" | "flag" | "arg" | "pipe" }

/** A one-line command as its words: each program (the first word, and the first after a pipe), its flags, its arguments. */
export function words(command: string): Word[] {
    let next: Word["kind"] = "program"
    return command.split(" ").filter(w => w !== "").map(text => {
        if (text === "|" || text === "&&") {
            next = "program"
            return { text: text, kind: "pipe" }
        }
        const kind: Word["kind"] = next === "program" ? "program" : text.startsWith("-") ? "flag" : "arg"
        next = "arg"
        return { text: text, kind: kind }
    })
}
