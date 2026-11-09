import { inject, reactive, type InjectionKey } from "vue"
import type { Account, BridgeHandle, Social, ItemInfo, Mode, PlanetariumHover, PlanetariumMessage, ScreenName, StateSections, UiValues } from "../bridge"

type Sections = { [S in keyof StateSections]: StateSections[S] | null }
type Ui = { [K in keyof UiValues]: UiValues[K] | null }

/**
 * Everything the engine has told the page, kept reactive. A section is null
 * until the engine first sends it: components render nothing rather than
 * invent a value.
 */
export function GameStore(opts: { bridge: BridgeHandle }) {
    const empty = (): Sections => ({ hud: null, player: null, mapPose: null, hotbar: null, tools: null, library: null, inventory: null })
    const noUi = (): Ui => ({ lab: null, wiki: null, discovered: null, probe: null, weather: null, travel: null, body: null, creative: null, flight: null, map: null, map_position: null, map_trajectory: null, world_controls: null, making: null, carrying: null })
    const store = reactive({
        mode: null as Mode | null,
        error: null as string | null,
        screen: null as ScreenName | null,
        /** The ArcLabs account; lives across menu and game. */
        account: null as Account | null,
        /** Friends, presence, invites; null while signed out (and before the engine says). */
        social: null as Social | null,
        items: {} as Record<string, ItemInfo>,
        state: empty(),
        /** What game Lua set for this page (player:set_ui), by key. */
        ui: noUi(),
        /** The last place clicked on the Planetarium, while it's shown. */
        sceneHover: null as number | null,
        picked: null as Omit<Extract<PlanetariumMessage, { event: "picked" }>, "kind" | "event"> | null,
        clock: null as { date: number; running: boolean } | null, // the planet viewer's clock, while it runs
        making: null as number | null, // a body the planet viewer is making: how far (0..1), till it's shown
        /** The body under the mouse on the Planetarium, while it's shown. */
        hover: null as PlanetariumHover | null,
        /** The last body a double click on the Planetarium flew to (its id; "star"), once: a page takes it. */
        chosen: null as string | null,
    })

    // Messages carry every change after this; the first answer fills the gap
    // when nothing has changed since the engine started.
    if (opts.bridge.connected) {
        void opts.bridge.send("account.get", {}).then((account) => { store.account ??= account })
        void opts.bridge.send("social.get", {}).then((social) => { store.social ??= social })
    }

    opts.bridge.on((message) => {
        switch (message.kind) {
            case "mode":
                store.mode = message.mode
                if (message.mode === "menu") {
                    store.state = empty() // the session is over
                    store.ui = noUi()
                    store.screen = null
                }
                break
            case "error":
                store.error = message.message
                break
            case "screen":
                store.screen = message.screen
                break
            case "scene":
                store.sceneHover = message.index
                break
            case "planetarium":
                switch (message.event) {
                    case "picked": store.picked = { latitude: message.latitude, longitude: message.longitude, cell: message.cell }; break
                    case "time": store.clock = { date: message.date, running: message.running }; break
                    case "hovered": store.hover = message.body; break
                    case "chosen": store.chosen = message.body; break
                    case "making": store.making = message.progress; break
                }
                break
            case "account":
                store.account = message.account
                break
            case "social":
                store.social = message.social
                break
            case "items":
                Object.assign(store.items, message.items)
                break
            case "state": // sections are replaced whole
                switch (message.section) {
                    case "hud": store.state.hud = message.value; break
                    case "player": store.state.player = message.value; break
                    case "mapPose": store.state.mapPose = message.value; break
                    case "hotbar": store.state.hotbar = message.value; break
                    case "tools": store.state.tools = message.value; break
                    case "library": store.state.library = message.value; break
                    case "inventory": store.state.inventory = message.value; break
                }
                break
            case "ui":
                switch (message.key) {
					case "map": store.ui.map = message.value; break
					case "map_position": store.ui.map_position = message.value; break
					case "map_trajectory": store.ui.map_trajectory = message.value; break
					case "world_controls": store.ui.world_controls = message.value; break
					case "creative": store.ui.creative = message.value; break
                    case "lab": store.ui.lab = message.value; break
                    case "weather": store.ui.weather = message.value; break
                    case "travel": store.ui.travel = message.value; break
                    case "body": store.ui.body = message.value; break
                    case "wiki": store.ui.wiki = message.value; break
                    case "discovered": store.ui.discovered = message.value; break
                    case "probe": store.ui.probe = message.value; break
                    case "flight": store.ui.flight = message.value; break
                    case "making": store.ui.making = message.value; break
                    case "carrying": store.ui.carrying = message.value; break
                }
                break
        }
    })

    return store
}

export type GameStoreHandle = ReturnType<typeof GameStore>

export const STORE: InjectionKey<GameStoreHandle> = Symbol("store")

export function useStore() {
    const store = inject(STORE)
    if (!store) throw new Error("useStore() outside the app: the store was not provided")
    return store
}
