import { createApp } from "vue"
import { isTauri } from "@tauri-apps/api/core"
import { getCurrentWebview } from "@tauri-apps/api/webview"
import App from "./App.vue"
import Fault from "./Fault.vue"
import { connect } from "./backend/index.ts"
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
    const store = LauncherStore({ backend: connect() })
    app.provide(STORE, store).mount("#app")
    store.load().catch(fail)
} catch (error) {
    fail(error)
}
