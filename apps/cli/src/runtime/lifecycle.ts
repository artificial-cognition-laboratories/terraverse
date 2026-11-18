import type { ManifestMod } from "../terra"
import type { ApiT } from "./api"
import type { ChannelT } from "./channel"
import type { GlobalsT } from "./globals"
import type { LoaderT } from "./loader"

type LifecycleOpts = {
    mods: readonly ManifestMod[]
    channel: ChannelT
    api: ApiT
    loader: LoaderT
    globals: GlobalsT
}

/**
 * Lifecycle — the sidecar's life: load every mod, send what they declared, then run each
 * window the engine opens until it says stop.
 */
export function Lifecycle(opts: LifecycleOpts) {
    async function run(): Promise<void> {
        // Ctrl-C in the terminal reaches the whole process group: the engine stops this
        // sidecar itself, in order (Op.stop), never a signal ahead of it
        const ignore = () => {}
        process.on("SIGINT", ignore)
        const restore = opts.globals.install()
        try {
            await opts.loader.load(opts.mods)
            opts.channel.content(opts.api.seal())
            for (;;) {
                const step = opts.channel.next()
                if (step === "stop") return
                await opts.channel.window(() => opts.api.step(step))
            }
        } finally {
            restore()
            process.off("SIGINT", ignore)
        }
    }

    return { run: run }
}

export type LifecycleT = ReturnType<typeof Lifecycle>
