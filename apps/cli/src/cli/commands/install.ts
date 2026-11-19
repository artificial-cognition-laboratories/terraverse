import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs"
import { join } from "node:path"
import { header, results, status } from "@arcforge/arcline"
import { err } from "../../errors"
import type { TerraT } from "../../terra"
import { parseSpec } from "../../terra/registry"
import { flags, type Args } from "../args"
import type { PresentationT } from "../presentation"
import type { PrepareCommandT } from "./prepare"

const major = (version: string) => version.split(".")[0]

/**
 * terra install <@scope/name[@version]>… [--yes] [--json] — published mods added to the
 * project you stand in, pinned to exact versions, then installed and checked by prepare.
 * If any of it fails, package.json and bunfig.toml are put back as they were.
 */
export function InstallCommand(opts: { terra: TerraT; presentation: PresentationT; prepare: PrepareCommandT }) {
    const p = opts.presentation

    async function run(args: Args): Promise<number> {
        flags.only(args, ["yes", "json"])
        const json = flags.on(args, "json")
        if (args.positional.length === 0) throw err("USAGE", { detail: "terra install <@scope/name[@version]>…" })
        const specs = args.positional.map(parseSpec)
        const project = opts.terra.projects.find(".")

        // every version is resolved before anything is written: a typo in the third mod
        // shouldn't leave the first two half-installed
        const chosen = []
        for (const spec of specs) chosen.push({ name: spec.name, version: await opts.terra.registry.resolve(spec.name, spec.version) })

        const kept = keep(project.root, ["package.json", "bunfig.toml"])
        try {
            const added = chosen.map(c => ({ ...c, previous: opts.terra.registry.add(project.root, c.name, c.version.version).previous }))
            const upgrades = added.filter(a => a.previous && major(a.previous) !== major(a.version.version))
            if (upgrades.length > 0 && !flags.on(args, "yes")) {
                throw err("USAGE", {
                    detail: `${upgrades.map(u => `${u.name} ${u.previous} → ${u.version.version}`).join(", ")} is a new major version: its author says it may not fit your worlds. Run again with --yes to go ahead`,
                })
            }
            if (json) await project.prepare()
            else await opts.prepare.prepare(project)

            if (json) {
                p.line(JSON.stringify(added.map(a => ({ name: a.name, version: a.version.version, previous: a.previous }))))
                return 0
            }
            p.line("")
            p.line(header(p.renderer, { title: "Installed", subtitle: project.root }))
            for (const row of results(p.renderer, added.map(a => ({
                name: `${a.name}@${a.version.version}`,
                outcome: "ok" as const,
                detail: a.version.title,
                ...(a.previous && a.previous !== a.version.version ? { note: `was ${a.previous}` } : {}),
            })))) p.line(`  ${row}`)
            return 0
        } catch (error) {
            kept.restore()
            if (!json) p.line(status(p.renderer, "info", "Nothing was installed", "package.json and bunfig.toml are as they were"))
            throw error
        }
    }

    return { run: run }
}

/** Files as they are now, to put back: a file that didn't exist is removed again. */
function keep(root: string, names: string[]) {
    const saved = names.map(name => {
        const path = join(root, name)
        return { path: path, text: existsSync(path) ? readFileSync(path, "utf8") : null }
    })
    return {
        restore(): void {
            for (const file of saved) {
                if (file.text === null) rmSync(file.path, { force: true })
                else writeFileSync(file.path, file.text)
            }
        },
    }
}
