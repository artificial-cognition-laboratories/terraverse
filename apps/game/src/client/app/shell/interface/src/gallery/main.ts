import { createApp } from "vue"
import { BRIDGE } from "../bridge/use.ts"
import { GameStore, STORE } from "../game/store.ts"
import "../theme.css"
import Gallery from "./Gallery.vue"
import { MOCK, MockBridge, swatch } from "./mock.ts"

const bridge = MockBridge()
const store = GameStore({ bridge: bridge })

bridge.emit({
    kind: "items",
    items: {
        "core:grass": { description: "Grass", icon: swatch("#5fa83a", "#6b4a2b"), mass: 1.5, faces: null },
        "core:dirt": { description: "Dirt", icon: swatch("#7a5533", "#6b4a2b"), mass: 1.5, faces: null },
        "core:stone": { description: "Stone", icon: swatch("#8a8a8a", "#6e6e6e"), mass: 2.7, faces: null },
        "core:sand": { description: "Sand", icon: swatch("#e3d39a", "#cdbb7e"), mass: 1.6, faces: null },
        "tf_tools:pick": { description: "Iron Pickaxe", icon: swatch("#c9c9c9", "#7b5a36"), mass: null, faces: null },
        "core:sandstone": { description: "Sandstone", icon: swatch("#d8c48a", "#bba56b"), mass: 2.3, faces: null },
        "core:stone_bricks": { description: "Stone Bricks", icon: swatch("#7a7a82", "#5d5d64"), mass: 2.5, faces: null },
    },
})

const app = createApp(Gallery)
app.provide(BRIDGE, bridge)
app.provide(STORE, store)
app.provide(MOCK, bridge)
app.mount("#app")
