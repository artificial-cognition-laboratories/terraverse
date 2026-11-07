import type { Component } from "vue"
import Account from "./Account.vue"
import Controls from "./Controls.vue"
import Create from "./Create.vue"
import Creative from "./Creative.vue"
import Hud from "./Hud.vue"
import Inventory from "./Inventory.vue"
import Making from "./Making.vue"
import Panels from "./Panels.vue"
import Screens from "./Screens.vue"
import World from "./World.vue"

/** Gallery pages, in tab order. */
export const PAGES = [
    { id: "controls", label: "Controls", component: Controls },
    { id: "panels", label: "Panels", component: Panels },
    { id: "hud", label: "HUD", component: Hud },
    { id: "inventory", label: "Inventory", component: Inventory },
    { id: "making", label: "Making", component: Making },
    { id: "creative", label: "Creative", component: Creative },
    { id: "world", label: "World", component: World },
    { id: "create", label: "Create", component: Create },
    { id: "account", label: "Account", component: Account },
    { id: "screens", label: "Screens", component: Screens },
] as const satisfies readonly { id: string; label: string; component: Component }[]

export type PageId = (typeof PAGES)[number]["id"]
