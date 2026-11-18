import type { ManifestData } from "../terra"
import { Api } from "./api"
import { Channel } from "./channel"
import { Globals } from "./globals"
import { Lifecycle } from "./lifecycle"
import { Loader } from "./loader"
import { Running } from "./running"
import type { WireT } from "./wire"

type RuntimeOpts = {
    /** The prepared server's mods. */
    manifest: ManifestData
    /** The socket to the engine. A test passes its own. */
    wire: WireT
}

/**
 * Runtime — the sidecar: TypeScript mods, run beside the engine. The engine starts it
 * as `terra runtime` with a socket on fd 3, opens a window each server step, and answers
 * its calls inside one.
 */
export function Runtime(opts: RuntimeOpts) {
    const channel = Channel({ wire: opts.wire })
    const running = Running({ channel: channel })
    const api = Api({ channel: channel, running: running, mods: opts.manifest.mods })
    const loader = Loader({ running: running })
    const globals = Globals({ api: api, channel: channel, running: running })
    const lifecycle = Lifecycle({ mods: opts.manifest.mods, channel: channel, api: api, loader: loader, globals: globals })

    return {
        run: lifecycle.run,
        api: api,
        channel: channel,
    }
}

export type RuntimeT = ReturnType<typeof Runtime>
