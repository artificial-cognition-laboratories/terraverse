import { dirname } from "node:path"
import { Art } from "./art"
import { Check } from "./check"
import { Dev, Scratch } from "./dev"
import { Engine } from "./engine"
import { Home } from "./home"
import { Projects } from "./project"
import { Running } from "./running"
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
 * running the engine on a server, what mods draw, and your account and the registry. Prints nothing: the CLI presents, this does.
 */
export function Terra(opts: TerraOpts) {
    const projects = Projects({ bun: opts.exec })
    const home = Home({ env: process.env, platform: process.platform, release: process.env.TERRAVERSE_RELEASE === "1" })
    const running = Running({ dir: home.running(), owner: home.handle })
    const engine = Engine({ bin: opts.engine ?? dirname(opts.exec), running: running })
    const scratch = Scratch({ projects: projects, cache: home.cache })
    const dev = Dev({ projects: projects, engine: engine, scratch: scratch })
    const art = Art({ projects: projects, scratch: scratch, share: engine.share })
    const check = Check({ projects: projects, art: art, bun: opts.exec })
    const arclabs = (opts.arclabs ?? process.env.ARCLABS_URL ?? DEFAULT_ARCLABS).replace(/\/+$/, "")
    const account = Account({
        base: arclabs,
        file: () => accountFile(home.profile(), arclabs, DEFAULT_ARCLABS),
        token: process.env.TERRA_TOKEN ?? null,
        signedIn: (handle, file) => void home.signedIn(handle, file),
        signedOut: home.signedOut,
    })
    const registry = Registry({ api: account.api })

    return {
        projects: projects,
        engine: engine,
        running: running,
        home: home,
        dev: dev,
        art: art,
        check: check,
        account: account,
        registry: registry,
    }
}

export type TerraT = ReturnType<typeof Terra>
