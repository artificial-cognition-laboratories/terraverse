import { dirname } from "node:path"
import { Dev } from "./dev"
import { Engine } from "./engine"
import { Places } from "./places"
import { Projects } from "./project"
import { Account, DEFAULT_ARCLABS, Registry, accountFile } from "./registry"

type TerraOpts = {
    /**
     * Where this terra is: the engine is installed beside it, and it runs itself as bun.
     * process.execPath, from whoever builds this.
     */
    exec: string
    /** The folder the engine is in, when it isn't beside exec: a build tree, for terral. */
    engine?: string
    /** The ArcLabs API: ARCLABS_URL, or production. Tests point it at local staging. */
    arclabs?: string
}

/**
 * Terra — the work behind the terra command: projects of every kind, preparing them,
 * running the engine on a server, and your account and the registry. Prints nothing: the CLI presents, this does.
 */
export function Terra(opts: TerraOpts) {
    const projects = Projects({ bun: opts.exec })
    const engine = Engine({ bin: opts.engine ?? dirname(opts.exec) })
    const places = Places({ env: process.env, platform: process.platform })
    const dev = Dev({ projects: projects, engine: engine, places: places })
    const arclabs = (opts.arclabs ?? process.env.ARCLABS_URL ?? DEFAULT_ARCLABS).replace(/\/+$/, "")
    const account = Account({ base: arclabs, file: accountFile(places.config(), arclabs, DEFAULT_ARCLABS), token: process.env.TERRA_TOKEN ?? null })
    const registry = Registry({ api: account.api })

    return {
        projects: projects,
        engine: engine,
        dev: dev,
        account: account,
        registry: registry,
    }
}

export type TerraT = ReturnType<typeof Terra>
