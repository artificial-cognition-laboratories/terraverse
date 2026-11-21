import { timer } from "@arcforge/arcline"
import { err } from "../../errors"
import type { TerraT } from "../../terra"
import { parseSpec } from "../../terra/registry"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"

const major = (version: string) => version.split(".")[0]

/**
 * terra install <@scope/name[@version]>… [--yes] [--json] — published mods added to the
 * project you stand in, pinned to exact versions, then installed and checked by prepare.
 * If any of it fails, package.json and bunfig.toml are put back as they were.
 */
export function InstallCommand(opts: { terra: TerraT; presentation: PresentationT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        flags.only(args, ["yes", "json"])
        if (args.positional.length === 0) throw err("USAGE", { detail: "terra install <@scope/name[@version]>…" })
        const specs = args.positional.map(parseSpec)
        const project = opts.terra.projects.find(".")

        // every version is resolved before anything is written: a typo in the third mod
        // shouldn't leave the first two half-installed
        const chosen = []
        for (const spec of specs) chosen.push({ name: spec.name, version: await opts.terra.registry.resolve(spec.name, spec.version) })

        const started = timer.start()
        const surface = p.surfaces.install(project.root, chosen.map(c => `${c.name}@${c.version.version}`))
        const kept = project.snapshot(["package.json", "bunfig.toml"])
        try {
            const added = chosen.map(c => ({ ...c, previous: opts.terra.registry.add(project.root, c.name, c.version.version).previous }))
            const upgrades = added.filter(a => a.previous && major(a.previous) !== major(a.version.version))
            if (upgrades.length > 0 && !flags.on(args, "yes")) {
                throw err("USAGE", {
                    detail: `${upgrades.map(u => `${u.name} ${u.previous} → ${u.version.version}`).join(", ")} is a new major version: its author says it may not fit your worlds. Run again with --yes to go ahead`,
                })
            }
            surface.preparing()
            await project.prepare()

            if (p.mode.machine) p.emit(JSON.stringify(added.map(a => ({ name: a.name, version: a.version.version, previous: a.previous }))))
            surface.finish({
                modules: added.map(a => ({
                    name: `${a.name}@${a.version.version}`,
                    outcome: a.previous === a.version.version ? ("noop" as const) : ("ok" as const),
                    detail: a.version.title,
                    ...(a.previous && a.previous !== a.version.version ? { note: `was ${a.previous}` } : {}),
                })),
                ms: timer.end(started),
                ...(project.kind === "server" ? { next: "terra host" } : {}),
            })
            return 0
        } catch (error) {
            kept.restore()
            throw surface.fail(error)
        }
    }

    return { run: run }
}
