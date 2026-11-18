import { Configs } from "./configs"
import { ConfigEditor } from "./editor"
import { Frame } from "./frame"
import { Install } from "./install"
import { KINDS, type ProjectKind } from "./kinds"
import { Packages } from "./packages"
import { Prepare } from "./prepare"
import { Server } from "./server"
import { Typegen } from "./typegen"

type ProjectOpts = {
    root: string
    kind: ProjectKind
    /** The program to run as bun, for installs: terra itself. */
    bun: string
}

/**
 * Project — one project of any kind on disk. Pure composition: the verbs are owned by
 * the leaves, and every kind gets the same surface. What a kind is lives in KINDS; the
 * one thing beyond it is `server`, the server kind's extension, null for the others.
 */
export function Project(opts: ProjectOpts) {
    const frame = Frame({ root: opts.root })
    const packages = Packages({})
    const configs = Configs({})
    const install = Install({ root: opts.root, bun: opts.bun })
    const typegen = Typegen({ frame: frame })
    const server = KINDS[opts.kind].runs ? Server({ root: opts.root, frame: frame, packages: packages, configs: configs, typegen: typegen }) : null
    const prepare = Prepare({
        root: opts.root,
        kind: opts.kind,
        packages: packages,
        configs: configs,
        install: install,
        typegen: typegen,
        server: server,
    })

    const editor = ConfigEditor({ path: `${opts.root}/${KINDS[opts.kind].config}`, define: KINDS[opts.kind].define })

    return {
        root: opts.root,
        kind: opts.kind,
        frame: frame,
        /** Its config file, changed by machine: a value set or removed, the author's layout kept. */
        config: editor,
        prepare: prepare.run,
        server: server,
    }
}

export type ProjectT = ReturnType<typeof Project>
