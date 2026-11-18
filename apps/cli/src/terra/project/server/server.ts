import type { ConfigsT } from "../configs"
import { Frame, type FrameT } from "../frame"
import { settings } from "../settings"
import { Typegen, type TypegenT } from "../typegen"
import type { PackagesT } from "../packages"
import type { ManifestData, Mod, PrepareStep, ServerConfigData } from "../types"
import { ServerConfig } from "./config"
import { Build } from "./build"
import { Manifest } from "./manifest"
import { Mods } from "./mods"
import { Worlds } from "./worlds"

type ServerOpts = {
    root: string
    frame: FrameT
    packages: PackagesT
    configs: ConfigsT
    typegen: TypegenT
}

/**
 * Server — what a server project has that other kinds don't: the mods it runs, built for
 * the sidecar, and its worlds. Hangs off the Project, as the server kind's extension.
 */
export function Server(opts: ServerOpts) {
    const mods = Mods({ root: opts.root, packages: opts.packages, configs: opts.configs })
    const build = Build({ frame: opts.frame })
    const manifest = Manifest({ frame: opts.frame })
    const worlds = Worlds({ root: opts.root })
    const config = ServerConfig({ root: opts.root, configs: opts.configs })

    /** Its mods found, ordered and built, and the manifest written. */
    async function prepare(e: { name: string; progress: (step: PrepareStep) => void }): Promise<{ config: ServerConfigData; mods: Mod[]; manifest: ManifestData }> {
        const read = await config.read(e.name)
        e.progress("mods")
        const found = await mods.read()
        // what each mod runs with, checked before any of them does
        const chosen = settings.resolve(found, read.settings)
        await opts.typegen.settings(settings.types.server(found))
        // a local mod is edited here, so its editor needs its types as much as the server's
        for (const mod of found.filter(m => m.local)) {
            const builtOn = found.filter(m => mod.needs.includes(m.name))
            await Typegen({ frame: Frame({ root: mod.root }) }).write("mod", settings.types.mod(mod.settings, builtOn))
        }
        e.progress("build")
        const built = await build.run(found, chosen)
        const data: ManifestData = { version: 1, mods: built }
        await manifest.write(data)
        return { config: read, mods: found, manifest: data }
    }

    return {
        prepare: prepare,
        config: config,
        manifest: manifest,
        worlds: worlds,
    }
}

export type ServerT = ReturnType<typeof Server>
