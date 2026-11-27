import { createApp } from "vue"
import { isTauri } from "@tauri-apps/api/core"
import { getCurrentWebview } from "@tauri-apps/api/webview"
import App from "./App.vue"
import Fault from "./Fault.vue"
import { BRIDGE } from "@game/bridge/use.ts"
import { connect } from "./backend/index.ts"
import { LauncherBridge } from "./bridge.ts"
import { LauncherStore, STORE } from "./store.ts"
import { Zoom } from "./zoom.ts"
import "@game/theme.css"
import "./launcher.css"

// Anything that stops the launcher starting is shown, never a blank window; it's still thrown for the console.
function fail(error: unknown): never {
    app.unmount()
    createApp(Fault, { error: error instanceof Error ? error.message : String(error) }).mount("#app")
    throw error
}

// the webview's own zoom in the app (it lays the page out at the new size); CSS's in a browser (bun dev, the mock backend)
const apply = isTauri()
    ? (scale: number) => void getCurrentWebview().setZoom(scale)
    : (scale: number) => document.documentElement.style.setProperty("zoom", String(scale))
Zoom({ apply: apply, storage: window.localStorage }).start()

const app = createApp(App)
try {
    const backend = connect()
    const store = LauncherStore({ backend: backend })
    // the game's account corner and popover, answered here (bridge.ts)
    app.provide(BRIDGE, LauncherBridge({ backend: backend, version: () => store.target.value?.id ?? null }))
    app.provide(STORE, store).mount("#app")
    store.load().catch(fail)
} catch (error) {
    fail(error)
}
