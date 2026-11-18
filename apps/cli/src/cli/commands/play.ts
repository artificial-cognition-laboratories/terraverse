import { status } from "@arcforge/arcline"
import { err } from "../../errors"
import { Exit } from "../../runtime"
import type { TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

/** What a reference is: decided once, by its shape. */
const REFS = {
    published: /^@[a-z0-9-]+\/[a-z0-9-]+(@.+)?$/,
    code: /^TF\d+\./,
} as const

/**
 * terra <ref> — open something straight away, in the game: a server folder (`terra .`) as
 * it is, or a world code as a clean universe with no mods. A published server waits for the
 * registry.
 */
export function PlayCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT }) {
    /** Whether `word`, in a command's place, is a reference to open. */
    function isRef(word: string): boolean {
        return word.startsWith(".") || word.startsWith("/") || REFS.published.test(word) || REFS.code.test(word)
    }

    async function run(args: Args): Promise<number> {
        const ref = args.command
        if (!ref) throw err("USAGE", { detail: "terra <ref>" })
        flags.only(args, ["world", "host"])
        if (flags.value(args, "world") !== undefined) throw err("NOT_WIRED", { detail: "landing on a chosen world isn't built yet" })
        if (flags.on(args, "host")) throw err("NOT_WIRED", { detail: "opening it to others while you play isn't built yet: terra host runs it for them" })
        if (REFS.published.test(ref)) throw err("NOT_WIRED", { detail: `opening ${ref} needs the registry, which isn't built yet` })
        if (REFS.code.test(ref)) return opts.terra.engine.run({ mode: "game", port: 0, code: ref })
        const project = opts.terra.projects.server(ref)
        for (;;) {
            const prepared = await opts.prepare.prepare(project)
            const code = await opts.terra.engine.run({ mode: "game", port: 0, server: prepared, world: project.server.worlds.folder })
            // its mods were prepared again with new content: it closed to be opened again
            if (code !== Exit.restart) return code
            opts.presentation.line(status(opts.presentation.renderer, "info", "Restarting the universe", "the mods' content changed"))
        }
    }

    return { isRef: isRef, run: run }
}
