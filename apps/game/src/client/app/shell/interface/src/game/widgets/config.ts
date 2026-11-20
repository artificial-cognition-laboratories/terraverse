import { reactive } from "vue"
import type { BridgeHandle } from "../../bridge"

export type WidgetCorner = 0 | 1 | 2 | 3

export const widgetConfig = reactive({
    ready: false,
    map: false,
    mapCorner: 0 as WidgetCorner,
    mapScale: 2,
    mapShape: 0,
    mapRotate: true,
    mapZoom: 3,
})

function corner(value: number | boolean | string | undefined): WidgetCorner {
    if (value === 0 || value === 1 || value === 2 || value === 3) return value
    throw new Error("Invalid widget corner setting")
}

export function applyWidgetSettings(values: Record<string, number | boolean | string>) {
    const map = values.widget_map
    const scale = values.widget_map_scale
    const shape = values.widget_map_shape
    const rotate = values.widget_map_rotate
    const zoom = values.widget_map_zoom
    if (typeof map !== "boolean" ||
        typeof scale !== "number" || !Number.isInteger(scale) || scale < 1 || scale > 3 ||
        typeof shape !== "number" || !Number.isInteger(shape) || shape < 0 || shape > 1 ||
        typeof rotate !== "boolean" || typeof zoom !== "number" || !Number.isInteger(zoom) || zoom < 1 || zoom > 5)
        throw new Error("Invalid widget settings")
    widgetConfig.map = map
    widgetConfig.mapCorner = corner(values.widget_map_corner)
    widgetConfig.mapScale = scale
    widgetConfig.mapShape = shape
    widgetConfig.mapRotate = rotate
    widgetConfig.mapZoom = zoom
    widgetConfig.ready = true
}

export async function loadWidgetSettings(bridge: BridgeHandle) {
    applyWidgetSettings(await bridge.send("settings.get", {}))
}
