import type { EngineT } from "../engine"
import type { ProjectsT } from "../project"
import type { ScratchT } from "./scratch"
import { Session } from "./session"
import type { DevEvent, DevOptions } from "./types"
import { Watch } from "./watch"

/** Dev — `terra dev`: a project run as it's edited, reloading as it's saved. */
export function Dev(opts: { projects: ProjectsT; engine: EngineT; scratch: ScratchT }) {
    const scratch = opts.scratch

    /** Run the nearest project at or above `from` until it's closed or Ctrl-C. */
    async function run(from: string, e: { options: DevOptions; report: (event: DevEvent) => void }): Promise<number> {
        const project = opts.projects.find(from)
        const server = await scratch.serverOf(project, { in: e.options.in })
        const watch = Watch({ dirs: [project.root] })
        const session = Session({ server: server, engine: opts.engine, watch: watch, options: e.options, report: e.report })
        return session.run()
    }

    return { run: run }
}

export type DevT = ReturnType<typeof Dev>
