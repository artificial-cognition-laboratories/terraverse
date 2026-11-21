import type { Args } from "../args"

/** Mode — who's reading: a person, or a program that asked for --json. Fixed once, before a command runs. */
export function Mode() {
    let machine = false

    return {
        configure(args: Args): void {
            machine = args.flags.json === true
        },
        /** A program is reading: surfaces draw nothing, and the command emits one JSON line. */
        get machine(): boolean {
            return machine
        },
    }
}

export type ModeT = ReturnType<typeof Mode>
