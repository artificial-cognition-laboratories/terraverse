import { watch } from "vue"
import type { BridgeHandle } from "../../bridge"
import type { GameStoreHandle } from "../store.ts"

/**
 * tf_craft offers making (its UI value appears): the page opens the screen.
 * The screen closed while it's still offered: tf_craft is told, and clears it.
 */
export function openMaking(opts: { bridge: BridgeHandle; store: GameStoreHandle }) {
    const { bridge, store } = opts
    watch(() => store.ui.making !== null, (offered) => {
        if (offered && store.screen !== "making") bridge.send("screen.open", { name: "making" }).catch(console.error)
    })
    watch(() => store.screen, (now, was) => {
        if (was === "making" && now !== "making" && store.ui.making)
            bridge.send("ui.send", { name: "making.leave", payload: {} }).catch(console.error)
    })
}
