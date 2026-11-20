import { shallowRef } from "vue"
import type { BridgeHandle, UiValues } from "../../bridge"
import { decodePreview, paint, type Fields, type OverlayId } from "../../worldgen/fields"

/** A map's world: its code, and (an atlas world's) which body. */
export type MapWorld = { code: string; body?: string }
/** One key for a map's world, the previews' and the paintings'. */
export function mapKey(world: MapWorld): string {
    return world.body ? `${world.code}@${world.body}` : world.code
}

type Preview = { key: string; fields: Fields }
const preview = shallowRef<Preview | null>(null)
const problem = shallowRef<string | null>(null)
let pending: { key: string; promise: Promise<void> } | null = null
let wanted = ""

/** A world's terrain (the create screen's), requested once per world and body in the game session. */
export function worldMapData() {
    async function ensure(bridge: BridgeHandle, world: MapWorld) {
        const key = mapKey(world)
        wanted = key
        if (preview.value?.key === key) return
        if (pending?.key === key) return pending.promise
        problem.value = null
        const request = world.body ? { code: world.code, body: world.body } : { code: world.code }
        const promise = bridge.send("worldgen.preview", { ...request, date: null, resolution: 512 })
            .then((wire) => {
                if (wanted === key) preview.value = { key, fields: decodePreview(wire) }
            })
            .catch((error: unknown) => {
                if (wanted === key) problem.value = error instanceof Error ? error.message : String(error)
            })
            .finally(() => { if (pending?.key === key) pending = null })
        pending = { key, promise }
        return promise
    }
    return { preview, problem, ensure }
}

export function mapImage(fields: Fields, map: UiValues["map"], overlay: OverlayId = "terrain"): ImageData {
    const image = paint(fields, overlay)
    if (map.mode === "creative") return image
    const bytes = Uint8Array.from(atob(map.explored), (character) => character.charCodeAt(0))
    if (bytes.length !== map.grid * map.grid / 8) throw new Error("Invalid exploration mask")
    const r = fields.resolution
    for (let y = 0; y < r; y++) {
        const row = Math.floor(y * map.grid / r)
        for (let x = 0; x < r; x++) {
            const col = Math.floor(x * map.grid / r)
            const bit = row * map.grid + col
            if (bytes[bit >> 3]! & (1 << (bit & 7))) continue
            const i = (y * r + x) * 4
            const shade = (x + y) % 9 === 0 ? 2 : 0
            image.data[i] = 10 + shade
            image.data[i + 1] = 18 + shade
            image.data[i + 2] = 24 + shade
            image.data[i + 3] = 255
        }
    }
    return image
}

/** World-model block coordinates → the create screen's equirectangular UV. */
export function mapUv(x: number, z: number, size: number) {
    const u = ((x + size / 2) / size % 1 + 1) % 1
    const v = Math.max(0, Math.min(1, (size / 2 - z) / size))
    return { u, v }
}

export function mapCoordinates(pose: { u: number; v: number }, size: number) {
    const u = ((pose.u % 1) + 1) % 1
    return { x: Math.round((u - 0.5) * size), z: Math.round((0.5 - pose.v) * size) }
}
