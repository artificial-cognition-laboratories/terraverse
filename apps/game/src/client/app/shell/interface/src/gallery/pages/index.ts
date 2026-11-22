import type { Component } from "vue"
import Account from "./Account.vue"
import Chat from "./Chat.vue"
import Contents from "./Contents.vue"
import Controls from "./Controls.vue"
import Create from "./Create.vue"
import Creative from "./Creative.vue"
import Debugger from "./Debugger.vue"
import Hud from "./Hud.vue"
import Kit from "./Kit.vue"
import Inventory from "./Inventory.vue"
import Library from "./Library.vue"
import Panels from "./Panels.vue"
import Play from "./Play.vue"
import Screens from "./Screens.vue"
import World from "./World.vue"

/** Gallery pages, in tab order. */
export const PAGES = [
    { id: "kit", label: "Kit", component: Kit },
    { id: "controls", label: "Controls", component: Controls },
    { id: "panels", label: "Panels", component: Panels },
    { id: "library", label: "Library", component: Library },
    { id: "hud", label: "HUD", component: Hud },
    { id: "inventory", label: "Inventory", component: Inventory },
    { id: "creative", label: "Creative", component: Creative },
    { id: "contents", label: "Contents", component: Contents },
    { id: "play", label: "Play", component: Play },
    { id: "world", label: "World", component: World },
    { id: "create", label: "Create", component: Create },
    { id: "account", label: "Account", component: Account },
    { id: "screens", label: "Screens", component: Screens },
    { id: "chat", label: "Chat", component: Chat },
    { id: "debugger", label: "Debugger", component: Debugger },
] as const satisfies readonly { id: string; label: string; component: Component }[]

export type PageId = (typeof PAGES)[number]["id"]
