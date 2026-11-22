<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { CreativeItem } from "../bridge/index.ts"
import { Button, Segmented, Turntable } from "../kit/index.ts"

/**
 * One block or item filling the stage: turned in 3D, or its pictures flat in 2D, with what
 * it is over the top corner and its pictures down the side. Zoom by wheel or the buttons.
 */
const props = defineProps<{ item: CreativeItem }>()

type View = "3d" | "2d"
const VIEWS = [{ value: "3d" as const, label: "3D" }, { value: "2d" as const, label: "2D" }]
const view = ref<View>("3d")
const zoom = ref(1)
/** In 2D, one picture alone; null for them all. */
const focus = ref<string | null>(null)

/** "workshop:stone_axe" → "workshop": who declared it. */
const modOf = (name: string) => name.slice(0, name.indexOf(":"))

/** Every picture it has: a block's faces as you'd read them, then the icon the inventory draws. */
const assets = computed(() => {
    const out: { label: string; src: string }[] = []
    const f = props.item.faces
    if (f) out.push({ label: "top", src: f.top }, { label: "side", src: f.side }, { label: "bottom", src: f.bottom })
    if (props.item.icon) out.push({ label: f ? "icon" : "item", src: props.item.icon })
    return out
})
const shown = computed(() => focus.value ? assets.value.filter(a => a.label === focus.value) : assets.value)
// a flat picture's edge at zoom 1: one alone gets the room, several share it
const flat = computed(() => Math.round((shown.value.length === 1 ? 320 : 160) * zoom.value))

watch(() => props.item.item, () => { zoom.value = 1; focus.value = null })
function pick(label: string) {
    focus.value = view.value === "2d" && focus.value === label ? null : label
    view.value = "2d"
}
function wheel(e: WheelEvent) {
    zoom.value = Math.min(8, Math.max(0.4, zoom.value * Math.exp(-e.deltaY * 0.0015)))
}
const step = (by: number) => { zoom.value = Math.min(8, Math.max(0.4, zoom.value * by)) }
</script>

<template>
    <div class="viewer">
        <Turntable v-if="view === '3d'" :key="item.item" v-model:zoom="zoom" class="scene" :faces="item.faces" :icon="item.icon" />
        <div v-else class="scene flat" @wheel.prevent="wheel">
            <figure v-for="a in shown" :key="a.label">
                <img :src="a.src" alt="" :style="{ width: `${flat}px`, height: `${flat}px` }">
                <figcaption>{{ a.label }}</figcaption>
            </figure>
        </div>

        <Segmented v-model="view" class="views" :options="VIEWS" size="sm" />

        <header class="about">
            <h4>{{ item.description }}</h4>
            <p><code>{{ item.item }}</code></p>
            <p>{{ item.faces ? "block" : "item" }} · {{ item.category }} · {{ modOf(item.item) === "core" ? "the game" : modOf(item.item) }}<template v-if="item.mass !== null"> · {{ item.mass }} kg</template></p>
        </header>

        <nav class="assets" aria-label="Its pictures">
            <button v-for="a in assets" :key="a.label" :class="{ on: view === '2d' && focus === a.label }" :title="a.label" @click="pick(a.label)">
                <img :src="a.src" alt=""><span>{{ a.label }}</span>
            </button>
        </nav>

        <div class="zoom">
            <Button size="sm" aria-label="Zoom out" @click="step(1 / 1.25)">−</Button>
            <button class="level" title="Back to fit" @click="zoom = 1">{{ Math.round(zoom * 100) }}%</button>
            <Button size="sm" aria-label="Zoom in" @click="step(1.25)">+</Button>
        </div>

        <p class="hint">{{ view === "3d" ? "Drag to turn · wheel to zoom · double-click to set it back" : "Wheel to zoom · pick a picture to see it alone" }}</p>
    </div>
</template>

<style scoped>
.viewer { position: relative; width: 100%; height: 100%; min-width: 0; min-height: 0; overflow: hidden; background: var(--stone-0); }
.scene { position: absolute; inset: 0; }
.flat { display: flex; flex-wrap: wrap; align-items: center; justify-content: center; align-content: center; gap: 24px; padding: 64px 120px; }
.flat figure { margin: 0; display: grid; gap: 6px; justify-items: center; }
.flat img { image-rendering: pixelated; box-shadow: var(--sunk); border: var(--b) solid var(--edge); background: repeating-conic-gradient(var(--stone-2) 0 25%, var(--stone-1) 0 50%) 0 0 / 16px 16px; }
figcaption, .assets span { color: var(--ink-faint); font: 10px var(--font-display); text-transform: uppercase; letter-spacing: 0.1em; }

.views { position: absolute; top: 14px; left: 14px; width: 120px; }
.about { position: absolute; top: 14px; right: 14px; max-width: 280px; text-align: right; pointer-events: none; }
.about h4 { margin: 0 0 4px; font: 400 16px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.about p { margin: 0; color: var(--ink-dim); font-size: 11px; }
.about code { color: var(--ink-faint); }

.assets { position: absolute; right: 14px; top: 50%; transform: translateY(-50%); display: grid; gap: 8px; }
.assets button { display: grid; justify-items: center; gap: 3px; padding: 4px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--raised); cursor: pointer; }
.assets button.on { border-color: var(--accent); box-shadow: var(--sunk); }
.assets img { width: 44px; height: 44px; image-rendering: pixelated; }

.zoom { position: absolute; right: 14px; bottom: 14px; display: flex; align-items: center; gap: 4px; }
.level { min-width: 52px; border: 0; background: none; color: var(--ink-dim); font: 11px var(--font-display); cursor: pointer; }
.hint { position: absolute; bottom: 18px; left: 0; right: 0; margin: 0; text-align: center; color: var(--ink-faint); font-size: 11px; pointer-events: none; }
</style>
