import { computed, inject, ref, watch, type InjectionKey } from "vue"
import type { BridgeHandle } from "../../bridge"
import { body as bodyOf, STAR } from "../../system/model"
import type { SystemPreviewHandle } from "../../system/preview"
import { BodySurfaceCache } from "../../worldgen/bodySurface"
import { paint } from "../../worldgen/fields"
import { Previewer } from "../../worldgen/previewer"
import { WeatherPreview } from "../../worldgen/weather"
import type { WorldDraftHandle } from "./draft"

/**
 * How every body looks, for the tree and the viewer alike: the playable
 * world's generated terrain on the chosen date (terragen's preview), and the
 * others' sketches from what they became (their cards).
 */
export function Looks(opts: { bridge: BridgeHandle; draft: WorldDraftHandle; cards: SystemPreviewHandle }) {
    const { draft, cards } = opts
    const date = ref(0.25) // northern midsummer
    const terrain = Previewer({ bridge: opts.bridge, resolution: 192 })
    watch(() => [draft.config, date.value], () => terrain.request(draft.config, date.value), { deep: true, immediate: true })
    // the weather only when asked for (about a second per world): the overlay calls want()
    const weather = WeatherPreview({ bridge: opts.bridge })
    const wanted = ref(false)
    watch(() => [draft.config, wanted.value], () => { if (wanted.value) weather.request(draft.config) }, { deep: true })

    const cache = BodySurfaceCache()
    const terrainImage = computed(() => (terrain.fields.value ? paint(terrain.fields.value, "terrain") : null))
    const surfaces = computed(() => {
        const out = new Map<string, ReturnType<typeof cache.get>>()
        for (const b of draft.system.bodies) {
            const card = cards.card(b.id)
            if (card) out.set(b.id, cache.get(b, card))
        }
        return out
    })

    function image(id: string): ImageData | null {
        if (id === STAR) return null
        if (bodyOf(draft.system, id).playable) return terrainImage.value
        return surfaces.value.get(id)?.image ?? null
    }

    return {
        date: date,
        terrain: terrain,
        weather: weather,
        wantWeather() { wanted.value = true },
        surfaces: surfaces,
        image: image,
    }
}

export type LooksHandle = ReturnType<typeof Looks>

export const LOOKS: InjectionKey<LooksHandle> = Symbol("looks")

export function useLooks() {
    const looks = inject(LOOKS)
    if (!looks) throw new Error("useLooks() outside the create screen")
    return looks
}
