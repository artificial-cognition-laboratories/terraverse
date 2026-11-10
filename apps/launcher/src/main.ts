import { createApp } from "vue"
import App from "./App.vue"
import Fault from "./Fault.vue"
import { connect } from "./backend/index.ts"
import { LauncherStore, STORE } from "./store.ts"
import "@game/theme.css"
import "./launcher.css"

// Anything that stops the launcher starting is shown, never a blank window; it's still thrown for the console.
function fail(error: unknown): never {
    app.unmount()
    createApp(Fault, { error: error instanceof Error ? error.message : String(error) }).mount("#app")
    throw error
}

const app = createApp(App)
try {
    const store = LauncherStore({ backend: connect() })
    app.provide(STORE, store).mount("#app")
    store.load().catch(fail)
} catch (error) {
    fail(error)
}
