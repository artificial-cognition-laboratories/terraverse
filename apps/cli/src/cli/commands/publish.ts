import { bytes, header, results, status, timer } from "@arcforge/arcline"
import { err } from "../../errors"
import type { TerraT } from "../../terra"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

/**
 * terra publish [folder] [--dry-run] [--json] — the mod you stand in, onto the registry
 * under your account's scope, at the version in its package.json. Prepared first, so it
 * goes up exactly as it runs; each step counts while it works.
 */
export function PublishCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        flags.only(args, ["dry-run", "json"])
        const project = opts.terra.projects.find(args.positional[0] ?? ".")
        if (project.kind !== "mod")
            throw err("NOT_WIRED", { detail: `publishing a ${project.kind} isn't built yet: a server publishes once worlds can travel as their changes` })
        if (flags.on(args, "dry-run")) return dry(project)
        // signed out is found before the work, not after it
        if (!opts.terra.account.signedIn()) throw err("SIGNED_OUT", { detail: "terra publish" })

        const started = timer.start()
        const surface = p.surfaces.publish((await project.pkg()).name)
        try {
            surface.preparing()
            await project.prepare()
            const published = await opts.terra.registry.publish(project.root, { progress: step => surface.publishing(step) })
            if (p.mode.machine) p.emit(JSON.stringify({ ...published, media: published.media.length }))
            surface.finish({
                version: published.version,
                assets: published.media.map(f => ({ path: f.path, size: bytes(f.bytes) })),
                ms: timer.end(started),
            })
            return 0
        } catch (error) {
            throw surface.fail(error)
        }
    }

    /** --dry-run: what would go up, checked and listed. Nothing leaves the machine. */
    async function dry(project: { root: string; prepare(): Promise<unknown> }): Promise<number> {
        await project.prepare()
        const checked = await opts.terra.registry.check(project.root)
        if (p.mode.machine) {
            p.emit(JSON.stringify({ name: checked.name, version: checked.version, api: checked.api, files: checked.files, media: checked.media, dryRun: true }))
            return 0
        }
        const r = p.renderer
        const rows = (files: Array<{ path: string; bytes: number }>) => results(r, files.map(f => ({ name: f.path, outcome: "noop" as const, detail: bytes(f.bytes) })))
        p.line("")
        p.line(header(r, { title: `${checked.name}@${checked.version}`, subtitle: `would publish · mod API ${checked.api}` }))
        p.line("")
        for (const row of rows(checked.files)) p.line(`  ${row}`)
        if (checked.media.length > 0) {
            p.line("")
            p.line(`  ${r.c.dim("README media")}`)
            for (const row of rows(checked.media)) p.line(`  ${row}`)
        }
        p.line("")
        p.line(status(r, "info", "Nothing was published", "--dry-run"))
        return 0
    }

    return { run: run }
}
