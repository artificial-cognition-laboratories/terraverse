import { header, next, results, status } from "@arcforge/arcline"
import { err } from "../../errors"
import type { TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

function size(bytes: number): string {
    if (bytes < 1024) return `${bytes} B`
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
    return `${(bytes / 1024 / 1024).toFixed(1)} MB`
}

/**
 * terra publish [folder] [--dry-run] [--json] — the mod you stand in, onto the registry
 * under your account's scope, at the version in its package.json. Prepared first, so it
 * goes up exactly as it runs.
 */
export function PublishCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        flags.only(args, ["dry-run", "json"])
        const dry = flags.on(args, "dry-run")
        const json = flags.on(args, "json")
        const project = opts.terra.projects.find(args.positional[0] ?? ".")
        if (project.kind !== "mod")
            throw err("NOT_WIRED", { detail: `publishing a ${project.kind} isn't built yet: a server publishes once worlds can travel as their changes` })
        // signed out is found before the work, not after it
        if (!dry && !opts.terra.account.signedIn()) throw err("SIGNED_OUT", { detail: "terra publish" })

        if (json) await project.prepare()
        else await opts.prepare.prepare(project)
        const checked = await opts.terra.registry.check(project.root)

        if (dry) {
            if (json) {
                p.line(JSON.stringify({ name: checked.name, version: checked.version, api: checked.api, files: checked.files, media: checked.media, dryRun: true }))
                return 0
            }
            const r = p.renderer
            p.line("")
            p.line(header(r, { title: `${checked.name}@${checked.version}`, subtitle: `would publish · mod API ${checked.api}` }))
            p.line("")
            for (const row of results(r, checked.files.map(f => ({ name: f.path, outcome: "noop" as const, detail: size(f.bytes) })))) p.line(`  ${row}`)
            if (checked.media.length > 0) {
                p.line("")
                p.line(`  ${r.c.dim("README media")}`)
                for (const row of results(r, checked.media.map(f => ({ name: f.path, outcome: "noop" as const, detail: size(f.bytes) })))) p.line(`  ${row}`)
            }
            p.line("")
            p.line(status(r, "info", "Nothing was published", "--dry-run"))
            return 0
        }

        const published = await opts.terra.registry.publish(project.root)
        if (json) {
            p.line(JSON.stringify(published))
            return 0
        }
        p.line(status(p.renderer, "ok", `Published ${published.name}@${published.version}`, `mod API ${published.api}${published.media ? ` · ${published.media} README media` : ""}`))
        p.line("")
        p.line(next(p.renderer, `terra install ${published.name}@${published.version}`))
        return 0
    }

    return { run: run }
}
