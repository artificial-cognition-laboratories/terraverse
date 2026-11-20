import { computed, shallowRef, watch, type Ref } from "vue"
import type { BodyCard, BridgeHandle, SystemWire } from "../../bridge"
import { System, body as bodyOf, type SystemHandle } from "../../system/model"
import type { StarId } from "../../worldgen/config"
import { STAR_CLASSES } from "../../system/stars"
import { BodySurfaceCache } from "../../worldgen/bodySurface"
import { paint, type Fields } from "../../worldgen/fields"

/**
 * A world's system as the map shows it: the engine's own (`system.world`:
 * the world code's system, every body's card), and how each body looks in
 * miniature: one with its land charted (`charted`) in its terrain, the rest
 * as sketches of their kind.
 */
type World = { system: SystemHandle; star: StarId; cards: Map<string, BodyCard> }
// per world code, for the session: the map's opened often and the answer never changes
const known = new Map<string, Promise<World>>()
const sketches = BodySurfaceCache()
export function worldOf(bridge: BridgeHandle, code: string) {
    let found = known.get(code)
    if (!found) {
        found = bridge.send("system.world", { code: code })
            .then((answer): World => ({ system: System(answer), star: starOf(answer), cards: new Map(answer.cards.map((c) => [c.id, c])) }))
        known.set(code, found)
    }
    return found
}

export function MapBodies(opts: { bridge: BridgeHandle; code: Ref<string | undefined>; charted: Ref<{ id: string; fields: Fields } | null> }) {
    const world = shallowRef<World | null>(null)
    const problem = shallowRef<string | null>(null)
    watch(opts.code, (code) => {
        world.value = null
        if (!code) return
        worldOf(opts.bridge, code)
            .then((answer) => {
                if (opts.code.value === code) world.value = answer
            })
            .catch((error: unknown) => {
                problem.value = error instanceof Error ? error.message : String(error)
                console.error("system.world", error)
            })
    }, { immediate: true })
    const terrain = computed(() => opts.charted.value ? { id: opts.charted.value.id, image: paint(opts.charted.value.fields, "terrain") } : null)
    function image(id: string): ImageData | null {
        if (terrain.value?.id === id) return terrain.value.image
        const w = world.value
        const card = w?.cards.get(id)
        return w && card ? sketches.get(bodyOf(w.system, id), card).image : null
    }
    return { world: world, problem: problem, image: image }
}

// the engine's star id, narrowed to one of the page's star types (worldgen/config.ts)
function starOf(system: SystemWire): StarId {
    const found = STAR_CLASSES.find((s) => s.id === system.star)
    if (!found) throw new Error(`system.world: no star type "${system.star}"`)
    return found.id
}
