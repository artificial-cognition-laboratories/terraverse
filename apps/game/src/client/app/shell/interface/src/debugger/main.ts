import { createApp } from "vue"
import DebuggerApp from "./DebuggerApp.vue"
import { Bridge } from "../bridge/index.ts"
import { Keyboard } from "../bridge/keyboard.ts"
import { BRIDGE } from "../bridge/use.ts"
import "../theme.css"

// The debugger's own page, beside the game's (shell/debugger.h): its own bridge, and no game store.
const bridge = Bridge()
if (!bridge.connected) throw new Error("debugger.html runs inside the engine; the gallery shows it against the mock")

const app = createApp(DebuggerApp)
app.provide(BRIDGE, bridge)
app.mount("#app")
Keyboard({ bridge: bridge })
bridge.send("ready", {})
