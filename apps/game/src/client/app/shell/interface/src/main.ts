import { createApp } from "vue"
import App from "./App.vue"
import { Bridge } from "./bridge/index.ts"
import { Keyboard } from "./bridge/keyboard.ts"
import { PagePerf } from "./bridge/pagePerf.ts"
import { Zoom } from "./bridge/zoom.ts"
import { BRIDGE } from "./bridge/use.ts"
import { GameStore, STORE } from "./game/store.ts"
import "./theme.css"

const bridge = Bridge()
const store = GameStore({ bridge: bridge })

const app = createApp(App)
app.provide(BRIDGE, bridge)
app.provide(STORE, store)
app.mount("#app")

if (bridge.connected) {
    Keyboard({ bridge: bridge })
    PagePerf({ bridge: bridge })
    Zoom({ bridge: bridge })
    bridge.send("ready", {})
}
