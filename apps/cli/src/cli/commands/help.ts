import type { PresentationT } from "../presentation"

/** terra help, and terra version. */
export function HelpCommand(opts: { presentation: PresentationT; version: string }) {
    async function help(): Promise<number> {
        const r = opts.presentation.renderer
        const lines = [
            `${r.c.bold(r.c.primary("terra"))} ${r.c.dim(opts.version)}`,
            "",
            `  terra server init <name>         ${r.c.dim("a new server, ready to host")}`,
            `  terra mod init <name>            ${r.c.dim("a new mod, ready to run (--scope @you to publish it as yours)")}`,
            `  terra <ref>                      ${r.c.dim("open it in the game: . or a server folder, or a world code")}`,
            `  terra prepare [folder]           ${r.c.dim("make the nearest server or mod ready")}`,
            `  terra dev [folder]               ${r.c.dim("play the nearest server or mod, reloading as you save")}`,
            `      --headless [--port n]        ${r.c.dim("no window: join it from the game")}`,
            `      --fresh                      ${r.c.dim("an empty save every run")}`,
            `      --in <server>                ${r.c.dim("a mod, inside a copy of a real server")}`,
            `  terra host [folder] [--port n]   ${r.c.dim("prepare the nearest server, then run it")}`,
            "",
            r.c.dim("Not built yet: install, uninstall, snapshot, publish, deploy."),
        ]
        for (const line of lines) opts.presentation.line(line)
        return 0
    }

    async function version(): Promise<number> {
        opts.presentation.line(opts.version)
        return 0
    }

    return { help: help, version: version }
}
