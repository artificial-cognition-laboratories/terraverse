import { err } from "../../errors"
import type { TerraT } from "../../terra"
import type { Args } from "../args"
import type { PresentationT } from "../presentation"
import { HelpCommand } from "./help"
import { DevCommand } from "./dev"
import { HostCommand } from "./host"
import { InitCommand } from "./init"
import { PlayCommand } from "./play"
import { PrepareCommand } from "./prepare"
import { RuntimeCommand } from "./runtime"
import { AccountCommand } from "./account"
import { InfoCommand } from "./info"
import { InstallCommand } from "./install"
import { PublishCommand } from "./publish"

type Command = (args: Args) => Promise<number>

/** Commands — terra's grammar: each command's name, and what runs it. */
export function Commands(opts: { terra: TerraT; presentation: PresentationT; version: string }) {
    const help = HelpCommand({ presentation: opts.presentation, version: opts.version })
    const prepare = PrepareCommand({ terra: opts.terra, presentation: opts.presentation })
    const host = HostCommand({ terra: opts.terra, presentation: opts.presentation, prepare: prepare })
    const runtime = RuntimeCommand({ terra: opts.terra })
    const init = InitCommand({ terra: opts.terra, presentation: opts.presentation })
    const dev = DevCommand({ terra: opts.terra, presentation: opts.presentation })
    const play = PlayCommand({ terra: opts.terra, presentation: opts.presentation, prepare: prepare })
    const account = AccountCommand({ terra: opts.terra, presentation: opts.presentation })
    const publish = PublishCommand({ terra: opts.terra, presentation: opts.presentation })
    const install = InstallCommand({ terra: opts.terra, presentation: opts.presentation })
    const info = InfoCommand({ terra: opts.terra, presentation: opts.presentation })

    const table: Record<string, Command> = {
        server: init.of("server"),
        mod: init.of("mod"),
        prepare: prepare.run,
        dev: dev.run,
        host: host.run,
        runtime: runtime.run,
        login: account.login,
        logout: account.logout,
        whoami: account.whoami,
        publish: publish.run,
        install: install.run,
        i: install.run,
        info: info.info,
        deprecate: info.deprecate,
        help: help.help,
        "--help": help.help,
        "-h": help.help,
        version: help.version,
        "--version": help.version,
        "-v": help.version,
        ...Object.fromEntries(UNWIRED.map(name => [name, () => Promise.reject(err("NOT_WIRED", { detail: `terra ${name} isn't built yet` }))])),
    }

    /** The command `args` names. No command is help. */
    function find(args: Args): Command {
        if (args.command === undefined) return help.help
        const command = table[args.command]
        if (command) return command
        // not a command: a reference to open (terra ., terra TF24.…, terra @you/game)
        if (play.isRef(args.command)) return play.run
        throw err("COMMAND_UNKNOWN", { detail: `terra ${args.command}` })
    }

    return { find: find }
}

export type CommandsT = ReturnType<typeof Commands>

/** Designed (see the docs' CLI pages), not built. */
const UNWIRED = ["uninstall", "snapshot", "deploy"] as const
