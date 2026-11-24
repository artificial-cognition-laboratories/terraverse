import { status } from "@arcforge/arcline"
import { err } from "../../errors"
import { Exit } from "../../runtime"
import { refs, SCHEME, type Ref, type TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

/**
 * terra <ref> — open something straight away, in the game: a server folder (`terra .`) as
 * it is, or a world code as a clean universe with no mods. A published server waits for the
 * registry.
 */
export function PlayCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT }) {
    /** Whether `word`, in a command's place, is a reference to open. */
    function isRef(word: string): boolean {
        return isPath(word) || parsed(word) !== null
    }

    /**
     * A ref in a command's place. An address must say it is one (the scheme, or a port):
     * a bare word is a command, never a host.
     */
    function parsed(word: string): Ref | null {
        const ref = refs.parse(word)
        if (ref?.kind === "address" && !word.startsWith(SCHEME) && !word.includes(":")) return null
        return ref
    }

    async function run(args: Args): Promise<number> {
        const ref = args.command
        if (!ref) throw err("USAGE", { detail: "terra <ref>" })
        flags.only(args, ["world", "host"])
        if (flags.value(args, "world") !== undefined) throw err("NOT_WIRED", { detail: "landing on a chosen world isn't built yet" })
        if (flags.on(args, "host")) throw err("NOT_WIRED", { detail: "opening it to others while you play isn't built yet: terra serve runs it for them" })
        if (!isPath(ref)) return open(parsed(ref) ?? usage(ref))
        const project = opts.terra.projects.server(ref)
        for (;;) {
            const prepared = await opts.prepare.prepare(project)
            const code = await opts.terra.engine.run({ mode: "game", port: 0, server: prepared, world: project.server.worlds.folder })
            // its mods were prepared again with new content: it closed to be opened again
            if (code !== Exit.restart) return code
            opts.presentation.line(status(opts.presentation.renderer, "info", "Restarting the universe", "the mods' content changed"))
        }
    }

    /** A ref, in the game. */
    function open(ref: Ref): Promise<number> {
        switch (ref.kind) {
            case "code": return opts.terra.engine.run({ mode: "game", port: 0, code: ref.code })
            case "published": throw err("NOT_WIRED", { detail: `opening ${refs.print(ref)} needs the registry, which isn't built yet` })
            case "friend": throw err("NOT_WIRED", { detail: `joining ${refs.print(ref)} from terra isn't built yet: join from the game's Play list` })
            case "address": throw err("NOT_WIRED", { detail: `joining ${refs.print(ref)} from terra isn't built yet` })
        }
    }

    return { isRef: isRef, run: run }
}

const isPath = (word: string) => word.startsWith(".") || word.startsWith("/")
const usage = (word: string): never => { throw err("USAGE", { detail: `${word} isn't a ref: a folder, a world code or a terra:// address` }) }
