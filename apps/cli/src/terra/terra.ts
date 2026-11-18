import { dirname } from "node:path"
import { Dev } from "./dev"
import { Engine } from "./engine"
import { Places } from "./places"
import { Projects } from "./project"

type TerraOpts = {
    /**
     * Where this terra is: the engine is installed beside it, and it runs itself as bun.
     * process.execPath, from whoever builds this.
     */
    exec: string
    /** The folder the engine is in, when it isn't beside exec: a build tree, for terral. */
    engine?: string
}

/**
 * Terra — the work behind the terra command: projects of every kind, preparing them, and
 * running the engine on a server. Prints nothing: the CLI presents, this does.
 */
export function Terra(opts: TerraOpts) {
    const projects = Projects({ bun: opts.exec })
    const engine = Engine({ bin: opts.engine ?? dirname(opts.exec) })
    const places = Places({ env: process.env, platform: process.platform })
    const dev = Dev({ projects: projects, engine: engine, places: places })

    return {
        projects: projects,
        engine: engine,
        dev: dev,
    }
}

export type TerraT = ReturnType<typeof Terra>
