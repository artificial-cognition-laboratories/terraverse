import type { ConfigsT } from "./configs"
import type { InstallT } from "./install"
import { KINDS, type ProjectKind } from "./kinds"
import { settings } from "./settings"
import type { PackagesT } from "./packages"
import type { TypegenT } from "./typegen"
import type { ServerT } from "./server"
import type { PrepareInput, Prepared } from "./types"

type PrepareOpts = {
    root: string
    kind: ProjectKind
    packages: PackagesT
    configs: ConfigsT
    install: InstallT
    typegen: TypegenT
    /** The server kind's extension: its mods, built for the sidecar. null for other kinds. */
    server: ServerT | null
}

/**
 * Prepare — a project made ready: installed if package.json changed, its config read,
 * and, for a server, its mods built. What it returns is the only thing that runs.
 */
export function Prepare(opts: PrepareOpts) {
    async function run(e: PrepareInput = {}): Promise<Prepared> {
        const progress = e.progress ?? (() => {})
        const installed = KINDS[opts.kind].installs && opts.install.needed()
        if (installed) {
            progress("install")
            await opts.install.run()
        }
        progress("config")
        const pkg = await opts.packages.read(`${opts.root}/package.json`)
        const config = await opts.configs.read(opts.kind, opts.root, pkg.name)
        progress("types")
        // a mod's own settings; a server's are its mods', written once it has read them
        await opts.typegen.write(opts.kind, opts.server ? settings.types.server([]) : settings.types.mod(config.settings))
        const base = { root: opts.root, name: pkg.name, title: config.title, installed: installed }
        if (!opts.server) return { ...base, kind: "mod" }
        const built = await opts.server.prepare({ name: pkg.name, progress: progress })
        return { ...base, title: built.config.title, kind: "server", config: built.config, mods: built.mods, manifest: built.manifest }
    }

    return { run: run }
}

export type PrepareT = ReturnType<typeof Prepare>
