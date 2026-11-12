// A stand-in engine for the component gallery: canned replies to every
// intent, and emit() to push engine messages through the real store.

import { inject, type InjectionKey } from "vue"
import { BridgeError } from "../bridge"
import { FailureCode } from "../bridge/types"
import type { Account, Player, Social, SandboxFrame, BodyCard, BodyKind, SystemWire, CommunityCatalog, CreativeItem, EngineMessage, MatterElement, MatterSubstance, IntentKind, Intents, LabScenario, LabValue, Loading, LoadingStage, PublicServer, ServerDirectory, SlotRef, Stack, StateSections, UiValues, World } from "../bridge"
import { DEFAULTS, decodeWorldCode, encodeWorldCode } from "../worldgen/config"
import { mockPreview, mockWeather } from "./mockWorld"
import { bindingScancode } from "../settings/bindings"

// The gallery's stand-in for the engine's home system (src/terragen/system.cpp: SystemSpec::home): no engine here.
export const CANNED_HOME: SystemWire = {
    star: "G", starMass: 1, metallicity: 0, age: 4.5, activity: 0.5, companion: { kind: "none", class: "K", mass: 0.75, separation: 0.2 },
    bodies: [
        { id: "cinder", name: "Cinder", parent: null, color: "#a76c55", phase: 0.18, orbit: 0.42, eccentricity: 0.2, mass: -1.22, formed: 0.1, volatiles: 0.05, day: 1400, tilt: 1, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "earth", name: "Earth", parent: null, color: "#50a4c5", phase: 0.62, orbit: 1, eccentricity: 0.017, mass: 0, formed: 0.35, volatiles: 0.6, day: 24, tilt: 23, playable: true, life: true, spin: "free", inclination: 0, land: null },
        { id: "moon", name: "Moon", parent: "earth", color: "#a8a8a1", phase: 0.1, orbit: 60, eccentricity: 0.055, mass: -1.91, formed: 0.35, volatiles: 0.05, day: 655, tilt: 7, playable: false, life: false, spin: "free", inclination: 5, land: null },
        { id: "vesper", name: "Vesper", parent: null, color: "#b47d5e", phase: 0.88, orbit: 1.6, eccentricity: 0.09, mass: -0.9, formed: 0.45, volatiles: 0.5, day: 25, tilt: 25, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "aster", name: "Aster", parent: "vesper", color: "#9a9188", phase: 0.4, orbit: 6, eccentricity: 0.02, mass: -2.4, formed: 0.5, volatiles: 0.05, day: 30, tilt: 0, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "jovian", name: "Jovian", parent: null, color: "#d7ad83", phase: 0.35, orbit: 5.2, eccentricity: 0.05, mass: 2.5, formed: 0.8, volatiles: 0.6, day: 10, tilt: 3, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "pyre", name: "Pyre", parent: "jovian", color: "#ce9464", phase: 0.2, orbit: 5.9, eccentricity: 0.0041, mass: -1.82, formed: 0.55, volatiles: 0.05, day: 42, tilt: 0, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "nacre", name: "Nacre", parent: "jovian", color: "#c9d6da", phase: 0.55, orbit: 9.4, eccentricity: 0.009, mass: -2.1, formed: 0.8, volatiles: 0.6, day: 85, tilt: 0, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "hale", name: "Hale", parent: "jovian", color: "#a9b9b4", phase: 0.75, orbit: 15, eccentricity: 0.0013, mass: -1.6, formed: 0.8, volatiles: 0.5, day: 172, tilt: 0, playable: false, life: false, spin: "free", inclination: 0, land: null },
        { id: "morrow", name: "Morrow", parent: "jovian", color: "#868a91", phase: 0.05, orbit: 26, eccentricity: 0.007, mass: -1.73, formed: 0.85, volatiles: 0.4, day: 400, tilt: 0, playable: false, life: false, spin: "free", inclination: 0, land: null },
    ],
}

type Handlers = { [K in IntentKind]: (request: Intents[K]["request"]) => Intents[K]["response"] }

let sandboxTime = 0
function cannedSandbox(time: number): SandboxFrame {
    const n = 27, bottom = (i: number) => i < 9
    const t = Array.from({ length: n }, (_, i) => (i === 4 ? Math.min(373, 293 + time) : 293))
    return {
        size: [3, 3, 3], time: time,
        main: Array.from({ length: n }, (_, i) => (bottom(i) ? 0 : -1)),
        state: Array.from({ length: n }, () => "liquid"),
        fill: Array.from({ length: n }, (_, i) => (bottom(i) ? 0.6 : 0)),
        temperature: t, activity: Array.from({ length: n }, () => 0),
        vapour: Array.from({ length: n }, (_, i) => (i === 13 && time > 80 ? 0 : -1)),
        haze: Array.from({ length: n }, (_, i) => (i === 13 && time > 80 ? 0.4 : 0)),
    }
}

/** A stand-in for tf_player/controls.lua: one of each kind, on the pages it has. */
export function mockWorldControls(): UiValues["world_controls"] {
    return { mode: "creative", pages: [
        { id: "time", title: "Time", icon: "time", sections: [
            { title: "Time of day", hint: "On this planet", controls: [
                { id: "time.noon", kind: "button", label: "Noon" }, { id: "time.dusk", kind: "button", label: "Dusk" },
                { id: "time.hour", kind: "slider", label: "Set time", min: 0, max: 0.99, step: 0.01, format: "clock", value: 0.5 },
            ] },
            { title: "Clock", hint: "For everyone", controls: [
                { id: "time.passes", kind: "toggle", label: "Time passes", value: true },
                { id: "time.speed", kind: "choice", label: "Speed", value: 1, options: [{ value: 1, label: "×1" }, { value: 10, label: "×10" }, { value: 100, label: "×100" }] },
            ] },
            { title: "Date", controls: [{ id: "time.length", kind: "fact", label: "Length of the day", value: "30.0 h" }] },
        ] },
        { id: "player", title: "Player", icon: "player", sections: [
            { title: "Moving", controls: [{ id: "player.fly", kind: "toggle", label: "Fly", setting: "free_move" }] },
        ] },
        { id: "travel", title: "Travel", icon: "travel", sections: [{ title: "Places", controls: [{ id: "travel.save", kind: "button", label: "Save this spot" }] }] },
        { id: "world", title: "World", icon: "world", sections: [{ title: "Rules", controls: [{ id: "world.damage", kind: "fact", label: "Damage", value: "On" }] }] },
        { id: "items", title: "Items", icon: "items", sections: [{ title: "Inventory", controls: [{ id: "items.clear", kind: "button", label: "Clear inventory", confirm: true, tone: "danger" }] }] },
        { id: "tuning", title: "Tuning", icon: "tuning", sections: [{ title: "Wall kick", controls: [
            { id: "tuning.kick_out", kind: "slider", label: "Out from the wall", setting: "tune_kick_out", min: 0, max: 12, step: 0.1, default: 4.5, unit: "n/s" },
            { id: "tuning.reset", kind: "button", label: "Back to defaults", resets: true },
        ] }] },
        { id: "view", title: "View", icon: "view", sections: [{ title: "Overlays", controls: [
            { id: "view.bounds", kind: "choice", label: "Block bounds", setting: "view_block_bounds", options: [{ value: "off", label: "Off" }, { value: "near", label: "Nearby" }] },
        ] }] },
    ] }
}

export function MockBridge() {
    const listeners = new Set<(message: EngineMessage) => void>()
    let worlds: World[] = [
        { name: "Pine Ridge", path: "/worlds/pine_ridge", code: encodeWorldCode({ ...DEFAULTS, seed: "pine ridge" }),
            mode: "survival", damage: true, pinned: true, lastOpened: 1792500000,
            rules: { tf_difficulty: "normal", tf_seasons: "true", tf_weather: "wild", tf_hunger: "normal" } },
        { name: "Saltwater", path: "/worlds/saltwater", code: encodeWorldCode({ ...DEFAULTS, seed: "saltwater" }),
            mode: "creative", damage: false, pinned: false, lastOpened: 1791000000,
            rules: { tf_difficulty: "peaceful", tf_seasons: "false", tf_weather: "mild" } },
    ]
    const worldCodes = new Map(worlds.map((world) => [world.path, world.code!]))
    const publicServers: PublicServer[] = [
        { id: "amber-crossing", name: "Amber Crossing", description: "A shared survival world shaped by seasons, trade routes, and towns.", host: "ArcLabs",
            region: "Europe", players: 34, capacity: 80, pingMs: 42, version: "0.1", tags: ["Survival", "Towns", "Hardy"],
            mods: ["tf_world", "tf_weather"], featured: true, favorite: true, compatible: true },
        { id: "iron-valley", name: "Iron Valley", description: "A cooperative iron-age settlement with a working rail network.", host: "Wren",
            region: "North America", players: 11, capacity: 32, pingMs: 106, version: "0.1", tags: ["Co-op", "Industry"],
            mods: ["tf_world", "lanterns"], featured: true, favorite: false, compatible: true },
        { id: "old-stone", name: "Old Stone", description: "A long-running world on an older build.", host: "Juno",
            region: "Europe", players: 7, capacity: 24, pingMs: 53, version: "0.0", tags: ["Survival", "Exploration"],
            mods: ["tf_world"], featured: false, favorite: false, compatible: false },
    ]
    const community: CommunityCatalog = {
        mods: [
            { id: "games/terrafirma/mods/tf_world", name: "tf_world", title: "World", description: "Base terrain, water, biomes, and trees.", author: "ArcLabs", source: "builtIn", depends: ["tf_lab"], enabledWorlds: [], configurable: false },
            { id: "games/terrafirma/mods/tf_weather", name: "tf_weather", title: "Weather", description: "Climate-driven clouds, rain, snow, and ash.", author: "ArcLabs", source: "builtIn", depends: ["tf_world"], enabledWorlds: [], configurable: false },
            { id: "mods/lanterns", name: "lanterns", title: "Lanterns", description: "Small placeable lanterns for the gallery.", author: "Wren", source: "installed", depends: ["tf_world"], enabledWorlds: ["/worlds/pine_ridge"], configurable: true },
        ],
        textures: [
            { id: "/textures/stonework", name: "Stonework", description: "A gallery example of an installed texture pack.", author: "Ash", active: false },
            { id: "/textures/clear_skies", name: "Clear Skies", description: "A gallery example with bright skies and quieter terrain.", author: "Juno", active: false },
        ],
        worlds: [],
    }
    const communityView = (): CommunityCatalog => ({
        mods: community.mods.map((mod) => ({ ...mod, enabledWorlds: [...mod.enabledWorlds] })),
        textures: community.textures.map((pack) => ({ ...pack })),
        worlds: worlds.map((world) => ({ ...world, code: worldCodes.get(world.path) ?? null })),
    })
    const settings: Record<string, number | boolean | string> = {
        viewing_range: 200, fov: 72, fps_max: 60, mouse_sensitivity: 0.2, sound_volume: 0.8,
        music_volume: 0.45, undersampling: 1, display_gamma: 1, pause_on_lost_focus: false,
        screenshot_format: "png", screenshot_quality: 0, fog_start: 0.4, 
        fullscreen: false, vsync: false, enable_dynamic_shadows: true,
        smooth_lighting: true, shadow_filters: "1", shadow_map_texture_size: "2048", shadow_map_max_distance: 140, shadow_soft_radius: 5,
        shadow_strength_gamma: 1, shadow_map_texture_32bit: true, shadow_map_color: false,
        enable_translucent_foliage: false, leaves_style: "fancy", mip_map: false,
        bilinear_filter: false, trilinear_filter: false, anisotropic_filter: false,
        antialiasing: "none", fsaa: "2", fxaa: false, connected_glass: false,
        enable_post_processing: true, tone_mapping: false,
        enable_auto_exposure: false, exposure_compensation: 0, debanding: true,
        enable_volumetric_lighting: false, enable_waving_leaves: false,
        enable_waving_plants: false, 
        enable_dig_particles: true, performance_tradeoffs: false,
        occlusion_culler: "bfs", enable_raytraced_culling: true,
        water_fine: true, water_fine_reach: 10, water_coarse_reach: 48, ground_detail_near: 12, ground_detail_far: 32,
        far_view: true, far_view_detail: 2, far_view_max: 16000,
        fps_max_unfocused: 10, sound_volume_unfocused: 0.3, 
        enable_bloom: false, enable_fog: true,
        invert_mouse: false, doubletap_jump: true,
        camera_smoothing: 0, arm_inertia: true, toggle_sneak_key: false, toggle_aux1_key: false,
        enable_hotbar_mouse_wheel: true,
        invert_hotbar_mouse_wheel: false, safe_dig_and_place: false, repeat_place_time: 0.25,
        joystick_frustum_sensitivity: 170, joystick_inner_deadzone: 0.25, joystick_outer_deadzone: 0,
        mute_sound: false, hurt_flash_enabled: true, 
        show_nametag_backgrounds: true,
        node_highlighting: "box", chat_font_size: 0, recent_chat_messages: 6,
        widget_map: true, widget_map_corner: 0, widget_map_scale: 2, widget_map_shape: 0,
        widget_map_rotate: true, widget_map_zoom: 3,
        widget_world_controls: true, widget_world_controls_corner: 1,
        free_move: false, fast_move: false, noclip: false, view_debug_text: false, view_movement: false, view_profiler: false,
        view_wireframe: false, view_bboxes: false, view_block_bounds: "off", tune_kick_out: 4.5,
    }
    const worldControls: UiValues["world_controls"] = mockWorldControls()
    const defaultBindings: Record<string, string> = {
        keymap_forward: "SYSTEM_SCANCODE_26|GAMEPAD_AXIS_MINUS_1",
        keymap_backward: "SYSTEM_SCANCODE_22|GAMEPAD_AXIS_PLUS_1",
        keymap_left: "SYSTEM_SCANCODE_4|GAMEPAD_AXIS_MINUS_0",
        keymap_right: "SYSTEM_SCANCODE_7|GAMEPAD_AXIS_PLUS_0",
        keymap_jump: "SYSTEM_SCANCODE_44|GAMEPAD_BUTTON_0",
        keymap_sneak: "SYSTEM_SCANCODE_225|GAMEPAD_BUTTON_1",
        keymap_aux1: "SYSTEM_SCANCODE_224|GAMEPAD_BUTTON_2",
        keymap_freemove: "SYSTEM_SCANCODE_14|GAMEPAD_BUTTON_13",
        keymap_brake: "SYSTEM_SCANCODE_5",
        keymap_pour: "SYSTEM_SCANCODE_27",
        keymap_dig: "MOUSE_BUTTON_1|GAMEPAD_AXIS_PLUS_5",
        keymap_place: "MOUSE_BUTTON_3|GAMEPAD_AXIS_PLUS_4",
        keymap_inventory: "SYSTEM_SCANCODE_8|GAMEPAD_BUTTON_3",
        keymap_drop: "SYSTEM_SCANCODE_20|GAMEPAD_BUTTON_12",
        keymap_prone: "SYSTEM_SCANCODE_29",
        keymap_zoom: "SYSTEM_SCANCODE_19|GAMEPAD_BUTTON_11",
        keymap_chat: "SYSTEM_SCANCODE_23",
        keymap_lab: "SYSTEM_SCANCODE_15",
        keymap_planetarium: "SYSTEM_SCANCODE_64",
        keymap_wiki: "SYSTEM_SCANCODE_10",
        keymap_minimap: "SYSTEM_SCANCODE_25|GAMEPAD_BUTTON_4",
        keymap_toggle_hud: "SYSTEM_SCANCODE_58",
        keymap_camera_mode: "SYSTEM_SCANCODE_6",
        keymap_fullscreen: "SYSTEM_SCANCODE_68",
        keymap_screenshot: "SYSTEM_SCANCODE_69|GAMEPAD_BUTTON_14",
        keymap_mute: "SYSTEM_SCANCODE_16",
        keymap_hotbar_next: "SYSTEM_SCANCODE_17|GAMEPAD_BUTTON_10",
        keymap_hotbar_previous: "SYSTEM_SCANCODE_5|GAMEPAD_BUTTON_9",
        keymap_autoforward: "",
        keymap_increase_volume: "",
        keymap_decrease_volume: "",
        keymap_increase_viewing_range_min: "SYSTEM_SCANCODE_46",
        keymap_decrease_viewing_range_min: "SYSTEM_SCANCODE_45",
    }
    let bindings = { ...defaultBindings }
    function withKey(value: string, scancode: number | null) {
        return [scancode === null ? "" : `SYSTEM_SCANCODE_${scancode}`,
            ...value.split("|").filter((part) => part && !part.startsWith("SYSTEM_SCANCODE_"))]
            .filter(Boolean).join("|")
    }

    // The device flow, played out: the "browser" approves a few seconds after
    // signing in starts.
    let account: Account = { status: "signedOut" }
    let approval: ReturnType<typeof setTimeout> | null = null
    const setAccount = (next: Account) => {
        account = next
        emitAll({ kind: "account", account: account })
        setSocial(next.status === "signedIn" ? social ?? cannedSocial() : null)
    }

    // Friends as the backend would answer them: a few in different places.
    let social: Social | null = null
    const setSocial = (next: Social | null) => {
        social = next
        emitAll({ kind: "social", social: social })
    }
    const view = () => {
        if (!social?.view) throw new Error("mock: social action while signed out")
        return social.view
    }
    const refuse = (kind: IntentKind, message: string) => new BridgeError(kind, FailureCode.Refused, message)
    const STRANGERS: Player[] = [{ id: "00000000-0000-4000-8000-00000000000a", username: "wren", name: "Wren" }]

    // A tiny stand-in for the server's inventory rules: enough to click through.
    const lists: Record<string, { width: number; stacks: (Stack | null)[] }> = {
        main: { width: 9, stacks: Array(36).fill(null) },
        craft: { width: 2, stacks: Array(4).fill(null) },
        craftpreview: { width: 0, stacks: [null] },
        craftresult: { width: 0, stacks: [null] },
    }
    const RECIPES = [
        { needs: "tf_world:sand", makes: { item: "tf_world:sandstone", count: 1, wear: 0 } },
        { needs: "tf_world:stone", makes: { item: "tf_world:stone_bricks", count: 4, wear: 0 } },
    ]
    // The Lab as the game's lab.json files and tf_lab would answer it.
    const SCENARIOS: LabScenario[] = [
        {
            id: "water.spill", mod: "tf_world", title: "Water finds its level", size: { x: 7, y: 3, z: 7 },
            description: "A full basin with a gap in its front wall. Water runs out through the gap and spreads across the floor, thinning as it goes.",
            controls: [{ id: "gap", kind: "slider", label: "Gap", min: 1, max: 5, step: 1, default: 3, unit: "blocks" }],
        },
        {
            id: "falling.blocks", mod: "tf_world", title: "Loose blocks fall", size: { x: 5, y: 7, z: 5 },
            description: "A column hung in the air drops and piles on the floor.",
            controls: [
                { id: "material", kind: "choice", label: "Material", default: "tf_world:sand", options: [{ value: "tf_world:sand", label: "Sand" }, { value: "tf_world:gravel", label: "Gravel" }] },
                { id: "drop", kind: "button", label: "Drop again" },
            ],
        },
        {
            id: "sky.sun", mod: "tf_sky", title: "The sun's path", size: { x: 9, y: 6, z: 9 },
            description: "The solar sky for any latitude and day.",
            controls: [
                { id: "latitude", kind: "slider", label: "Latitude", min: -90, max: 90, step: 1, default: 50, unit: "°" },
                { id: "day", kind: "slider", label: "Day after the spring equinox", min: 0, max: 364, step: 1, default: 91, unit: "days" },
                { id: "noon", kind: "button", label: "Noon" },
            ],
        },
    ]
    let lab: UiValues["lab"] | null = null

    function labDefaults(id: string): Record<string, LabValue> {
        const values: Record<string, LabValue> = {}
        for (const c of SCENARIOS.find((s) => s.id === id)?.controls ?? []) if (c.kind !== "button") values[c.id] = c.default
        return values
    }

    function publishLab() {
        const value = lab ? { ...lab, values: { ...lab.values } } : null
        queueMicrotask(() => emitAll({ kind: "ui", key: "lab", value: value }))
    }

    // Canned body cards: one fixed card per kind, so the System tab can be
    // laid out without the engine. No physics here — that's terragen's alone.
    const CANNED: Record<string, BodyKind> = {
        cinder: "airless", earth: "temperate", moon: "airless", vesper: "cold desert", aster: "airless",
        jovian: "giant", pyre: "volcanic", nacre: "ice shell", hale: "ice shell", morrow: "airless",
    }
    function cannedCard(id: string, kind: BodyKind): BodyCard {
        const temperate = kind === "temperate"
        return {
            id: id, kind: kind, giant: kind === "giant", metal: 0.3, rock: 0.6, ice: kind === "ice shell" ? 0.5 : 0.1,
            radius: kind === "giant" ? 11 : temperate ? 1 : 0.3, gravity: kind === "giant" ? 2.5 : temperate ? 1 : 0.15,
            escape: temperate ? 11.2 : 2.4, sunlight: 1, temperature: temperate ? 288 : 210, equilibrium: 254,
            pressure: kind === "giant" ? null : temperate ? 1 : 0, gases: temperate ? { N2: 0.78, O2: 0.21 } : kind === "giant" ? { H2: 0.86, He: 0.14 } : {},
            field: temperate ? 1 : 0, heat: kind === "volcanic" ? 40 : 1, tidal: kind === "volcanic" ? 40 : 0,
            ocean: temperate, plates: temperate, volcanism: kind === "volcanic", locked: false, spin: "free", day: 24,
            layers: kind === "giant"
                ? [{ name: "core", outer: 0.12, molten: false }, { name: "metallic hydrogen", outer: 0.7, molten: true }, { name: "envelope", outer: 1, molten: true }]
                : [{ name: "core", outer: 0.5, molten: temperate }, { name: "mantle", outer: 0.97, molten: false }, { name: "crust", outer: 1, molten: false }],
            history: temperate ? [{ age: 0.1, text: "oceans condense (canned)" }, { age: 0.8, text: "life appears in the oceans (canned)" }] : [],
        }
    }

    // Canned matter: two elements and water, with fixed curve points, so the
    // Matter screen can be laid out. The physics is src/matter's alone.
    const ELEMENTS: MatterElement[] = [
        { number: 1, symbol: "H", name: "Hydrogen", mass: 1.008, category: "nonmetal", period: 1, group: 1, row: 1, col: 1, color: "#ffffff", radius: 31 },
        { number: 8, symbol: "O", name: "Oxygen", mass: 15.999, category: "nonmetal", period: 2, group: 16, row: 2, col: 16, color: "#ff0d0d", radius: 66 },
    ]
    const WATER: MatterSubstance = {
        id: "water", name: "Water", formula: "H₂O", kind: "compound", composition: { H: 2, O: 1 }, molar_mass: 18.015,
        triple: [273.16, 611.657], critical: [647.1, 22.064e6], boil: 373.15, fusion: 6.01, vaporization: 40.65,
        density: { solid: 917, liquid: 1000 }, conductivity: { solid: 2.2, liquid: 0.6, gas: 0.025 }, heat_capacity: 4186, approximate: false, note: "Canned for the gallery.",
        structure: { type: "molecule", atoms: [["O", 0, 0, 0], ["H", 0.757, 0.586, 0], ["H", -0.757, 0.586, 0]], bonds: [[0, 1, 1], [0, 2, 1]] },
        curves: {
            sublimation: [[136.6, 1e-4], [180, 0.05], [220, 3], [250, 76], [273.16, 611.657]],
            vapour: [[273.16, 611.657], [320, 10500], [373.15, 101325], [450, 932000], [550, 6.1e6], [647.1, 22.064e6]],
            melting: [[273.16, 611.657], [273.1, 1e6], [266, 1e8], [200, 1e9], [-466, 1e10]],
        },
    }

    const at = (s: SlotRef) => lists[s.list]!.stacks
    const emitAll = (message: EngineMessage) => listeners.forEach((l) => l(message))

    // The player's 2×2 crafting grid.
    function recipe() {
        const g = lists.craft!.stacks
        for (const r of RECIPES)
            if (g.every((s) => s?.item === r.needs)) return { recipe: r }
        return null
    }

    function publish() {
        const found = recipe()
        lists.craftpreview!.stacks = [found ? { ...found.recipe.makes } : null]
        const value: StateSections["inventory"] = JSON.parse(JSON.stringify(lists))
        emitAll({ kind: "state", section: "inventory", value: value })
    }

    function take(from: SlotRef, count: number): Stack | null {
        const stacks = at(from)
        const s = stacks[from.index]
        if (!s) return null
        const n = Math.min(count, s.count)
        stacks[from.index] = s.count - n > 0 ? { ...s, count: s.count - n } : null
        return { ...s, count: n }
    }

    function put(list: string, index: number, stack: Stack): Stack | null {
        const stacks = lists[list]!.stacks
        const there = stacks[index]
        if (!there) { stacks[index] = stack; return null }
        if (there.item === stack.item) {
            const room = 99 - there.count, n = Math.min(room, stack.count)
            stacks[index] = { ...there, count: there.count + n }
            return stack.count - n > 0 ? { ...stack, count: stack.count - n } : null
        }
        stacks[index] = stack
        return there // swapped out
    }

    function somewhere(list: string, stack: Stack): Stack | null {
        const stacks = lists[list]!.stacks
        let rest: Stack | null = stack
        for (let i = 0; i < stacks.length && rest; i++) if (stacks[i]?.item === rest.item) rest = put(list, i, rest)
        for (let i = 0; i < stacks.length && rest; i++) if (!stacks[i]) rest = put(list, i, rest)
        return rest
    }

    let making: ReturnType<typeof setInterval> | undefined // the create screen's body being made
    const handlers: Handlers = {
        ready: () => null,
        // no native planet in the gallery: the page around it is what's shown
        // (a body made over a few seconds, stage by stage, as the engine tells it: the create screen's bar)
        "planetarium.show": () => {
            clearInterval(making)
            let at = 0
            making = setInterval(() => {
                at += 0.3
                const loading = cannedLoading(at)
                emitAll({ kind: "planetarium", event: "making", loading })
                if (!loading) clearInterval(making)
            }, 300)
            return null
        },
        "planetarium.scene": () => null,
        "planetarium.orbits": () => null,
        "planetarium.hide": () => null,
        "planetarium.focus": () => null,
        "planetarium.markers": () => null,
        "planetarium.viewport": () => null,
        "planetarium.fly": () => null,
        "planetarium.frame": () => null,
        "planetarium.layer": () => null,
        "planetarium.time": () => null,
        "planetarium.seams": () => null,
        capture: () => null,
        "update.check": () => ({ current: "v0.1.1", latest: "0.1.2", available: true, launcher: false }),
        "worlds.list": () => worlds,
        "worlds.create": ({ name, code, settings }) => {
            const world: World = { name: name, path: `/worlds/${name.toLowerCase().replace(/\W+/g, "_")}`,
                code, mode: settings.creative_mode ? "creative" : "survival", damage: Boolean(settings.enable_damage),
                pinned: false, lastOpened: null,
                rules: Object.fromEntries(Object.entries(settings).filter(([key]) => key.startsWith("tf_")).map(([key, value]) => [key, String(value)])) }
            worlds = [...worlds, world]
            worldCodes.set(world.path, code)
            return world
        },
        "worlds.delete": ({ path }) => {
            worlds = worlds.filter((w) => w.path !== path)
            worldCodes.delete(path)
            return null
        },
        "worlds.rename": ({ path, name }) => {
            const world = worlds.find((w) => w.path === path)
            if (!world) throw refuse("worlds.rename", "that world no longer exists")
            world.name = name
            return null
        },
        "worlds.pin": ({ path, pinned }) => {
            const world = worlds.find((item) => item.path === path)
            if (!world) throw refuse("worlds.pin", "that world no longer exists")
            world.pinned = pinned
            return null
        },
        "servers.list": (): ServerDirectory => ({ status: "ready", servers: publicServers.map((server) => ({ ...server, tags: [...server.tags], mods: [...server.mods] })) }),
        "servers.favorite.set": ({ id, favorite }) => {
            const server = publicServers.find((item) => item.id === id)
            if (!server) throw refuse("servers.favorite.set", "that server no longer exists")
            server.favorite = favorite
            return null
        },
        "servers.join": ({ id }) => {
            const server = publicServers.find((item) => item.id === id)
            if (!server?.compatible) throw refuse("servers.join", "that server is not compatible with this build")
            return null
        },
        "community.list": () => communityView(),
        "community.mod.set": ({ id, world, enabled }) => {
            const mod = community.mods.find((item) => item.id === id)
            if (!mod || !mod.configurable) throw refuse("community.mod.set", "this mod cannot be changed")
            if (!worlds.some((item) => item.path === world)) throw refuse("community.mod.set", "that world no longer exists")
            mod.enabledWorlds = enabled ? [...new Set([...mod.enabledWorlds, world])] : mod.enabledWorlds.filter((path) => path !== world)
            return null
        },
        "community.texture.set": ({ id }) => {
            if (id && !community.textures.some((pack) => pack.id === id)) throw refuse("community.texture.set", "that texture pack is not installed")
            for (const pack of community.textures) pack.active = pack.id === id
            return null
        },
        play: () => null,
        "screen.open": ({ name }) => {
            queueMicrotask(() => emitAll({ kind: "screen", screen: name }))
            return null
        },
        "ui.send": (event) => {
            if (event.name === "map.teleport") return null // the gallery has no world to move in
            if (event.name === "controls.set") {
                for (const page of worldControls.pages) for (const section of page.sections) for (const control of section.controls) {
                    if (control.id !== event.payload.id) continue
                    if (control.kind === "toggle" && typeof event.payload.value === "boolean") control.value = event.payload.value
                    if (control.kind === "slider" && typeof event.payload.value === "number") control.value = event.payload.value
                    if (control.kind === "choice" && (typeof event.payload.value === "string" || typeof event.payload.value === "number")) control.value = event.payload.value
                }
                emitAll({ kind: "ui", key: "world_controls", value: { ...worldControls } })
                return null
            }
            if (event.name === "creative.take") {
                lists.main!.stacks[event.payload.slot] = { item: event.payload.item, count: event.payload.count, wear: 0 }
                publish()
                return null
            }
            if (event.name === "creative.clear") {
                lists.main!.stacks[event.payload.slot] = null
                publish()
                return null
            }
            if (event.name === "making.hand") { emitAll({ kind: "ui", key: "making", value: cannedMaking() }); return null }
            if (event.name === "making.leave") { emitAll({ kind: "ui", key: "making", value: null }); return null }
            if (event.name === "making.make") {
                const offer = cannedMaking().offers?.find((o) => o.id === event.payload.offer)
                if (!offer?.ok) return null
                emitAll({ kind: "ui", key: "making", value: { ...cannedMaking(), working: { id: offer.id, seconds: offer.seconds } } })
                setTimeout(() => emitAll({ kind: "ui", key: "making", value: cannedMaking() }), offer.seconds * 1000)
                return null
            }
            if (!lab) return null
            switch (event.name) {
                case "lab.control": if (event.payload.value !== null) lab.values[event.payload.id] = event.payload.value; break
                case "lab.restart": break
                case "lab.speed": lab.speed = event.payload.speed; break
                case "lab.browse": lab.browsing = true; break
                case "lab.step_in": lab.browsing = false; break
            }
            publishLab()
            return null
        },
        quit: () => null,
        "settings.get": () => ({ ...settings }),
        "settings.set": ({ key, value }) => {
            settings[key] = value
            return null
        },
        "settings.apply": ({ values }) => {
            Object.assign(settings, values)
            return null
        },
        "settings.bindings.get": () => ({ ...bindings }),
        "settings.bindings.set": ({ key, scancode, replace }) => {
            if (!(key in bindings)) throw refuse("settings.bindings.set", "unknown binding")
            const conflicts = Object.entries(bindings).filter(([name, value]) =>
                name !== key && bindingScancode(value) === scancode)
            if (conflicts.length && !replace) throw refuse("settings.bindings.set", `key already assigned to ${conflicts[0]![0]}`)
            for (const [name, value] of conflicts) bindings[name] = withKey(value, null)
            bindings[key] = withKey(bindings[key]!, scancode)
            return null
        },
        "settings.bindings.reset": () => { bindings = { ...defaultBindings }; return null },
        open: () => null,
        "account.get": () => account,
        "account.signIn": () => {
            setAccount({ status: "pending", code: "TFM-4K2", url: "https://axon.arclabs.it/auth/device?code=TFM-4K2&client=terrafirma" })
            approval = setTimeout(() => setAccount({ status: "signedIn", id: "u-1", username: "cody", name: "Cody" }), 4000)
            return null
        },
        "account.cancel": () => {
            if (approval) clearTimeout(approval)
            setAccount({ status: "signedOut" })
            return null
        },
        "account.signOut": () => { setAccount({ status: "signedOut" }); return null },
        "account.retry": () => { setAccount({ status: "signedIn", id: "u-1", username: "cody", name: "Cody" }); return null },
        "social.get": () => social,
        "social.open": ({ open }) => {
            if (!social?.joinable) throw refuse("social.open", "friends can only join your own world")
            setSocial({ ...social, me: { ...social.me, open: open } })
            return null
        },
        "social.request": ({ username }) => {
            const v = view()
            const known = [...v.friends, ...v.incoming, ...v.outgoing]
            if (username === "cody") throw refuse("social.request", "that's you")
            if (known.some((p) => p.username === username)) throw refuse("social.request", "already friends, or already asked")
            const stranger = STRANGERS.find((p) => p.username === username)
            if (!stranger) throw refuse("social.request", "no ArcLabs account has that username")
            setSocial({ ...social!, view: { ...v, outgoing: [...v.outgoing, stranger] } })
            return null
        },
        "social.accept": ({ id }) => {
            const v = view()
            const from = v.incoming.find((p) => p.id === id)
            if (!from) throw refuse("social.accept", "no request from that player")
            setSocial({ ...social!, view: { ...v, incoming: v.incoming.filter((p) => p.id !== id), friends: [...v.friends, { ...from, presence: { state: "menu", world: null, open: false } }] } })
            return null
        },
        "social.remove": ({ id }) => {
            const v = view()
            const keep = <T extends { id: string }>(list: T[]) => list.filter((p) => p.id !== id)
            setSocial({ ...social!, view: { friends: keep(v.friends), incoming: keep(v.incoming), outgoing: keep(v.outgoing), invites: v.invites.filter((i) => i.from.id !== id) } })
            return null
        },
        "social.invite": () => {
            if (social?.me.state === "menu") throw refuse("social.invite", "you're not in a world to invite them to")
            return null
        },
        // the real engine leaves for their world; the gallery has none to go to
        "social.join": ({ name }) => { throw refuse("social.join", `the gallery can't reach ${name}'s world: it has no engine`) },
        "social.dismiss": ({ id }) => {
            const v = view()
            setSocial({ ...social!, view: { ...v, invites: v.invites.filter((i) => i.id !== id) } })
            return null
        },
        "system.home": () => CANNED_HOME,
        "system.world": () => ({ ...CANNED_HOME, cards: CANNED_HOME.bodies.map((b) => cannedCard(b.id, CANNED[b.id] ?? "airless")) }),
        "system.preview": ({ bodies }) => bodies.map((b) => cannedCard(b.id, CANNED[b.id] ?? "airless")),
        "matter.list": () => ({ elements: ELEMENTS, substances: [WATER], reactions: [], rocks: [] }),
        // the mixture comes back as it went in: the gallery runs no chemistry
        "matter.react": ({ amounts, temperature }) => ({ amounts: amounts, temperature: temperature, states: {}, rates: {} }),
        // a canned sandbox: a pool of water warming under a heater (the gallery has no physics)
        "sandbox.create": () => { sandboxTime = 0; return { id: 1 } },
        "sandbox.step": ({ seconds }) => { sandboxTime += seconds; return cannedSandbox(sandboxTime) },
        "sandbox.drop": () => null,
        "moves.list": () => [{ id: "walk", group: "On foot", title: "Walking", keys: "W", note: "Mock: the moves are the engine's." }],
        "moves.show": () => null,
        "moves.set": () => null,
        "moves.restart": () => null,
        "moves.state": () => ({ time: 0, doing: "stand", pace: 0 }),
        "relief.show": () => null,
        "relief.set": () => null,
        "relief.restart": () => null,
        "relief.state": () => ({ round: 0, rounds: 12 }),
        "flora.show": () => null,
        // (the gallery grows nothing: one lineage's numbers, whatever the site)
        // (the gallery grows nothing: a few lineages and genes, whatever the site)
        "flora.set": ({ site, genes }) => ({
            lineages: [["Kreidei", "Broadleaf", "Trees and bushes"], ["Goune", "Split-leaf", "Palms and great leaves"], ["Steredei", "Fern", "Ferns, grasses, herbs"]]
                .map(([name, preset, group], i) => ({ name: name!, preset: preset!, group: group!, leaf: "#4f8a35", bark: "#6b5a48", lives: i !== 2 })),
            genes: [["height", "Height (m)", "Stems", 0.05, 45, false, 20], ["levels", "Branching levels", "Branches", 0, 3, true, 3], ["splits", "Splits", "Leaves", 0, 1, false, 0]]
                .map(([id, label, group, lo, hi, whole, value]) => ({ id: id as string, label: label as string, group: group as string, lo: lo as number, hi: hi as number,
                    whole: whole as boolean, value: genes[id as string] ?? value as number, grown: value as number, set: id as string in genes })),
            strategy: { warmth: (site.heat + 1) / 2, water: (site.wet + 1) / 2, light: site.light, food: 0.6 + 0.4 * site.fertility, rooting: 1, vigour: 0.5, disturbance: site.wind, strategy: "competitor", limit: "cold" },
            traits: { lives: true, why: "none", evergreen: false, leafing: 1, flowering: 0, leaf: "#4f8a35" },
            blocks: { wood: 40, branch: 65, leaf: 936 },
            grain: 2,
        }),
        "scene.show": () => null,
        "scene.place": () => null,
        "scene.hide": () => null,
        "orbit.coast": () => { throw new Error("orbit.coast is not wired in the gallery: it needs the engine") },
        "matter.state": () => ({ state: "liquid", boils_at: 373.15, melts_at: 273.15 }),
        "worldgen.preview": ({ code, date, resolution }) => mockPreview(decodeWorldCode(code), date, resolution),
        "weather.preview": ({ snapshots }) => mockWeather(snapshots),
        "screen.close": () => {
            queueMicrotask(() => listeners.forEach((l) => l({ kind: "screen", screen: null })))
            return null
        },
        "game.leave": () => {
            lab = null
            queueMicrotask(() => emitAll({ kind: "mode", mode: "menu" }))
            return null
        },
        "game.quit": () => null,
		"creative.list": (): CreativeItem[] => [
			{ item: "tf_world:grass", description: "Grass", icon: swatch("#5fa83a", "#6b4a2b"), stackMax: 64, category: "nature", mass: 1.5 },
			{ item: "tf_world:dirt", description: "Dirt", icon: swatch("#7a5533", "#6b4a2b"), stackMax: 64, category: "nature", mass: 1.5 },
			{ item: "tf_world:stone", description: "Stone", icon: swatch("#8a8a8a", "#6e6e6e"), stackMax: 64, category: "building", mass: 1.5 },
			{ item: "tf_world:sand", description: "Sand", icon: swatch("#e3d39a", "#cdbb7e"), stackMax: 64, category: "nature", mass: 1.5 },
			{ item: "tf_world:stone_bricks", description: "Stone Bricks", icon: swatch("#7a7a82", "#5d5d64"), stackMax: 64, category: "building", mass: 1.5 },
		],
        "inventory.move": ({ from, to, count }) => {
            const moving = take(from, count)
            if (!moving) return null
            const back = to.index === undefined ? somewhere(to.list, moving) : put(to.list, to.index, moving)
            if (back) put(from.list, from.index, back) // a swap: the displaced stack takes the emptied slot
            publish()
            return null
        },
        "inventory.craft": ({ count, collect }) => {
            for (let n = 0; n < count; n++) {
                const found = recipe()
                if (!found) break
                const r = lists.craftresult!.stacks[0]
                if (r && r.item !== found.recipe.makes.item) break
                for (const i of [0, 1, 2, 3])
                    take({ list: "craft", index: i }, 1)
                put("craftresult", 0, { ...found.recipe.makes })
            }
            if (collect) {
                const r = take({ list: "craftresult", index: 0 }, 999)
                if (r) somewhere("main", r)
            }
            publish()
            return null
        },
        "tools.pick": () => null,
        "tools.ask": () => null,
        "tools.brush": () => null,
        "hotbar.select": () => null,
        "inventory.drop": ({ from, count }) => {
            take(from, count)
            publish()
            return null
        },
    }

    return {
        connected: true,
        send<K extends IntentKind>(kind: K, request: Intents[K]["request"]): Promise<Intents[K]["response"]> {
            console.info(`[mock] ${kind}`, request)
            // a handler's throw is the engine refusing: a rejected promise, as the real bridge gives
            try {
                return Promise.resolve(handlers[kind](request))
            } catch (error) {
                return Promise.reject(error)
            }
        },
        on(listener: (message: EngineMessage) => void) {
            listeners.add(listener)
            return () => listeners.delete(listener)
        },
        emit: emitAll,
        /** Fill the mock inventory and publish it, as joining a world would. */
        stock(stacks: Record<number, Stack>) {
            for (const [i, s] of Object.entries(stacks)) lists.main!.stacks[Number(i)] = s
            publish()
        },
    }
}

/** What tf_craft offers at a workbench with flint, granite, a stick and two fibres in reach. */
export function cannedMaking(): UiValues["making"] {
    return {
        station: "Workbench",
        at: { x: 10, y: 4, z: -3 },
        worktop: { width: 3, stacks: [
            { item: "tf_craft:stone_flint", count: 1, wear: 0 }, false, false,
            { item: "tf_craft:stick_wood", count: 1, wear: 0 }, false, false,
            { item: "tf_craft:fibre_fibre", count: 2, wear: 0 }, false, false,
        ] },
        offers: [
            { id: "flake_flint", item: "tf_craft:flake_flint", label: "Flint flake", count: 2, process: "Knap", inputs: [{ label: "Flint", count: 1, have: 1 }], tool: "Stone", seconds: 2, ok: true },
            { id: "axe_head_flint", item: "tf_craft:axe_head_flint", label: "Flint axe head", count: 1, process: "Knap", inputs: [{ label: "Flint", count: 1, have: 1 }], tool: "Stone", seconds: 2, ok: true },
            { id: "axe_head_stone", label: "Stone axe head", count: 1, process: "Knap", inputs: [{ label: "Stone", count: 1, have: 1 }], seconds: 2, ok: false, reason: "Stone: only stone that breaks clean, like flint, knaps" },
            { id: "handle_wood", item: "tf_craft:handle_wood", label: "Handle", count: 1, process: "Carve", inputs: [{ label: "Stick", count: 1, have: 1 }], seconds: 3, ok: false, reason: "needs a blade harder than wood" },
            { id: "cord_fibre", item: "tf_craft:cord_fibre", label: "Cord", count: 1, process: "Twist", inputs: [{ label: "Plant fibre", count: 3, have: 2 }], seconds: 1.5, ok: false, reason: "needs 3 plant fibre, has 2" },
        ],
    }
}

/** A friends list in every state the panel draws. Also the gallery's specimen data. */
export function cannedSocial(opts: { inWorld?: boolean } = {}): Social {
    const id = (n: number) => `00000000-0000-4000-8000-00000000000${n}`
    return {
        me: opts.inWorld ? { state: "hosting", world: "Riverlands", open: false } : { state: "menu", world: null, open: false },
        joinable: opts.inWorld ?? false,
        view: {
            friends: [
                { id: id(1), username: "ash", name: "Ash", presence: { state: "hosting", world: "Saltwater", open: true } },
                { id: id(2), username: "bram", name: null, presence: { state: "hosting", world: "Pine Ridge", open: false } },
                { id: id(3), username: "juno", name: "Juno", presence: { state: "menu", world: null, open: false } },
                { id: id(4), username: "oskar", name: null, presence: null },
                { id: id(7), username: "wren", name: "Wren", presence: { state: "hosting", world: "Copper Coast", open: true } },
            ],
            incoming: [{ id: id(5), username: "pip", name: "Pip" }],
            outgoing: [{ id: id(6), username: "tamsin", name: null }],
            invites: [{ id: "10000000-0000-4000-8000-000000000001", from: { id: id(1), username: "ash", name: "Ash" }, world: "Saltwater", expiresAt: new Date(Date.now() + 3600_000).toISOString() }],
        },
    }
}

/** A flat 16×16 icon with a darker rim, as a data URL. */
export function swatch(top: string, side: string) {
    const c = document.createElement("canvas")
    c.width = c.height = 16
    const g = c.getContext("2d")!
    g.fillStyle = side
    g.fillRect(0, 0, 16, 16)
    g.fillStyle = top
    g.fillRect(1, 1, 14, 5)
    g.fillStyle = "rgb(0 0 0 / 0.35)"
    g.fillRect(0, 15, 16, 1)
    g.fillRect(15, 0, 1, 16)
    return c.toDataURL()
}

export type MockBridgeHandle = ReturnType<typeof MockBridge>
export const MOCK: InjectionKey<MockBridgeHandle> = Symbol("mock")

/** The mock engine, for gallery pages that drive state (emit) directly. */
export function useMock() {
    const mock = inject(MOCK)
    if (!mock) throw new Error("useMock() outside the gallery")
    return mock
}

/** A world being made `at` seconds in, as shell/loading.cpp tells it (layout kept); null once it's shown. */
export function cannedLoading(at: number): Loading | null {
    const STAGES: [id: string, label: string, seconds: number][] = [
        ["layout", "Laying out the system", 0], ["plates", "Moving the plates", 0.5], ["climate", "Setting the climate", 0.4],
        ["erosion", "Wearing the land", 0.2], ["rivers", "Tracing the rivers", 2.4], ["planet", "Seeing it from orbit", 1.5],
    ]
    const total = STAGES.reduce((sum, [, , seconds]) => sum + seconds, 0)
    if (at >= total) return null
    let begun = 0
    const stages = STAGES.map(([id, label, seconds]): LoadingStage => {
        const into = at - begun
        begun += seconds
        if (seconds === 0) return { id, label, state: "kept", seconds: 0, share: null }
        if (into >= seconds) return { id, label, state: "done", seconds, share: null }
        if (into < 0) return { id, label, state: "waiting", seconds: 0, share: null }
        return { id, label, state: "running", seconds: Math.round(into * 10) / 10, share: id === "rivers" ? Math.round(into / seconds * 100) / 100 : null }
    })
    return { share: Math.round(at / total * 100) / 100, stages }
}
