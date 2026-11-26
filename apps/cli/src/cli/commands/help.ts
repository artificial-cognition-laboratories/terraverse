import type { PresentationT } from "../presentation"

/** terra help, and terra version. */
export function HelpCommand(opts: { presentation: PresentationT; version: string }) {
    async function help(): Promise<number> {
        const r = opts.presentation.renderer
        const lines = [
            `${r.c.bold(r.c.primary("terra"))} ${r.c.dim(opts.version)}`,
            "",
            `  terra server init <name>         ${r.c.dim("a new server, ready to serve")}`,
            `  terra mod init <name>            ${r.c.dim("a new mod, ready to run (--scope @you to publish it as yours)")}`,
            `  terra <ref>                      ${r.c.dim("open it in the game: . or a server folder, or a world code")}`,
            `  terra prepare [folder]           ${r.c.dim("make the nearest server or mod ready")}`,
            `  terra dev [folder]               ${r.c.dim("play the nearest server or mod, reloading as you save")}`,
            `      --headless [--port n]        ${r.c.dim("no window: join it from the game")}`,
            `      --fresh                      ${r.c.dim("an empty save every run")}`,
            `      --in <server>                ${r.c.dim("a mod, inside a copy of a real server")}`,
            `  terra serve [folder] [--port n]  ${r.c.dim("prepare the nearest server, then run it")}`,
            `      --world <name>               ${r.c.dim("one of your worlds instead (terra worlds lists them)")}`,
            `  terra worlds                     ${r.c.dim("your worlds: the game's Play list")}`,
            `  terra check [folder]             ${r.c.dim("typecheck the nearest project, then load its mods dry: what's wrong and where")}`,
            `  terra art render [folder]        ${r.c.dim("draw every block and item its mods declare: a sheet, and each alone")}`,
            `      --out <dir> --match <text>   ${r.c.dim("where (default .terra/art), and only what matches")}`,
            `      --with <stone,dirt,…>        ${r.c.dim("the game's own blocks first on the sheet, to hold the art against")}`,
            `      --beside <mod folder,…>      ${r.c.dim("another mod's blocks and items first on the sheet, as a reference")}`,
            `  terra art list [folder]          ${r.c.dim("the same, as a list (--match, --json)")}`,
            "",
            `  terra install <@scope/name>…     ${r.c.dim("add published mods, pinned (@version for one; --yes for a major upgrade)")}`,
            `  terra publish [--dry-run]        ${r.c.dim("put the mod you stand in on the registry")}`,
            `  terra info <@scope/name>         ${r.c.dim("a published mod: its versions and what they need")}`,
            `  terra deprecate <@scope/name> <reason> | --undo`,
            `  terra login · logout · whoami    ${r.c.dim("your ArcLabs account (ARCLABS_URL picks the server); login opens your browser, --no-browser doesn't")}`,
            "",
            r.c.dim("Not built yet: uninstall, snapshot, deploy."),
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
