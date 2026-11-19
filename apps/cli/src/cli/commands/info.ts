import { header, results, status } from "@arcforge/arcline"
import { err } from "../../errors"
import type { TerraT } from "../../terra"
import { MOD_API } from "../../terra/project"
import { parseSpec } from "../../terra/registry"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra info <@scope/name> [--json] — a published mod as the registry describes it: who
 * owns it, every version, the mod API each was built against, and what's withheld.
 * terra deprecate <@scope/name> <reason> · terra deprecate <@scope/name> --undo.
 */
export function InfoCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation

    async function info(args: Args): Promise<number> {
        flags.only(args, ["json"])
        const [given] = args.positional
        if (!given) throw err("USAGE", { detail: "terra info <@scope/name>" })
        const { name } = parseSpec(given)
        const page = await opts.terra.registry.page(name)
        if (!page) throw err("MOD_NOT_PUBLISHED", { detail: name })
        if (flags.on(args, "json")) {
            p.line(JSON.stringify(page))
            return 0
        }

        const r = p.renderer
        const latest = page.versions[0]
        p.line("")
        p.line(header(r, { title: latest ? `${latest.title}  ${r.c.dim(name)}` : name, subtitle: page.owner ? `by @${page.owner}` : undefined }))
        if (page.description) p.line(`  ${page.description}`)
        if (page.deprecated !== null) p.line(status(r, "warn", "Deprecated", page.deprecated || "no reason given"))
        p.line("")
        for (const row of results(r, page.versions.map(v => ({
            name: v.version,
            outcome: v.withheld ? ("fail" as const) : v.api > MOD_API ? ("waiting" as const) : ("noop" as const),
            detail: `mod API ${v.api}`,
            ...(v.withheld ? { note: `withheld: ${v.withheld}` } : v.api > MOD_API ? { note: "needs a newer terra" } : {}),
        })))) p.line(`  ${row}`)
        return 0
    }

    async function deprecate(args: Args): Promise<number> {
        flags.only(args, ["undo"])
        const [given, ...reason] = args.positional
        if (!given) throw err("USAGE", { detail: "terra deprecate <@scope/name> <reason> (or --undo)" })
        const { name } = parseSpec(given)
        const undo = flags.on(args, "undo")
        const message = reason.join(" ").trim()
        if (undo && message) throw err("USAGE", { detail: "--undo lifts a deprecation; it takes no reason" })
        if (!undo && !message) throw err("USAGE", { detail: `say why: terra deprecate ${name} "use @you/furnaces instead"` })
        await opts.terra.registry.deprecate(name, undo ? null : message)
        p.line(status(p.renderer, "ok", undo ? `${name} is no longer deprecated` : `${name} is deprecated`, undo ? undefined : "hidden from search; it still installs, because worlds are saved in its names"))
        return 0
    }

    return { info: info, deprecate: deprecate }
}
