<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from "vue"
import { moonsOf, planets, STAR, type SystemHandle } from "./model.ts"
import { star, type StarId } from "../worldgen/config.ts"
import BodyIcon from "../worldgen/BodyIcon.vue"

/**
 * A system's bodies to choose from: the star, each planet as a little globe
 * of how it looks, its moons in a row under it. The builder's and the map's.
 * `tag` puts a word under a body (the builder's "play", the map's "you").
 */
const props = defineProps<{
    system: SystemHandle
    star: StarId
    selected: string
    image: (id: string) => ImageData | null
    tag?: (id: string) => string | null
}>()
const emit = defineEmits<{ pick: [id: string] }>()

// a long system in a small window: arrows, not a scrollbar over the planet
const row = ref<HTMLElement | null>(null)
const overflows = ref(false)
function measure() {
    if (row.value) overflows.value = row.value.scrollWidth > row.value.clientWidth + 1
}
const sizes = new ResizeObserver(measure)
onMounted(() => { if (row.value) sizes.observe(row.value) })
onBeforeUnmount(() => sizes.disconnect())
const scroll = (way: number) => row.value?.scrollBy({ left: way * 216 })
</script>

<template>
    <nav class="strip" aria-label="Choose a celestial body">
        <button v-if="overflows" class="arrow" aria-label="Earlier bodies" @click="scroll(-1)">‹</button>
        <div ref="row" class="bodies">
            <div class="body-block">
                <button class="body-button" :class="{ on: selected === STAR }"
                    :aria-current="selected === STAR ? 'true' : undefined" @click="emit('pick', STAR)">
                    <BodyIcon kind="star" :color="star(props.star).color" />
                    <span>{{ `Class ${props.star}` }}</span>
                </button>
            </div>
            <div v-for="planet in planets(system)" :key="planet.id" class="body-block">
                <button class="body-button" :class="{ on: selected === planet.id }"
                    :aria-current="selected === planet.id ? 'true' : undefined" @click="emit('pick', planet.id)">
                    <BodyIcon kind="planet" :image="image(planet.id)" />
                    <span>{{ planet.name }}</span>
                    <small v-if="tag?.(planet.id)">{{ tag(planet.id) }}</small>
                </button>
                <div v-if="moonsOf(system, planet.id).length" class="moon-row" :aria-label="`Moons of ${planet.name}`">
                    <button v-for="moon in moonsOf(system, planet.id)" :key="moon.id" :title="moon.name" :aria-label="moon.name"
                        :class="{ on: selected === moon.id, tagged: !!tag?.(moon.id) }" @click="emit('pick', moon.id)">
                        <BodyIcon :kind="image(moon.id) ? 'planet' : 'moon'" :image="image(moon.id)" />
                    </button>
                </div>
            </div>
        </div>
        <button v-if="overflows" class="arrow" aria-label="Later bodies" @click="scroll(1)">›</button>
    </nav>
</template>

<style scoped>
.strip { display: flex; align-items: stretch; gap: 8px; min-width: 0; padding: 5px 0; }
.bodies { display: flex; align-items: start; gap: 5px; flex: 1; min-width: 0; overflow-x: auto; scrollbar-width: none; }
.arrow { flex: none; width: 24px; border: var(--b) solid var(--edge); background: var(--stone-2); color: var(--ink); font: 14px var(--font-display); cursor: pointer; }
.arrow:hover { background: var(--stone-3); }
.body-block { flex: none; width: 72px; display: flex; flex-direction: column; align-items: center; gap: 2px; }
.body-button { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 2px; width: 100%; min-width: 0; padding: 2px; border: var(--b) solid transparent; background: none; color: var(--ink-dim); cursor: pointer; }
.body-button { min-height: 53px; }
.body-button:hover, .moon-row button:hover { background: var(--stone-2); color: var(--ink); }
.body-button.on, .moon-row button.on { border-color: var(--edge); background: var(--accent-lo); box-shadow: var(--sunk); color: var(--ink); }
.body-button span:not(.icon) { display: block; max-width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font: 9px var(--font-display); text-transform: uppercase; }
.body-button small { color: var(--accent-hi); font: 7px var(--font-display); text-transform: uppercase; }
.body-button :deep(.icon) { width: 24px; height: 24px; }
.moon-row { display: flex; flex-wrap: wrap; justify-content: center; gap: 1px; max-width: 72px; }
.moon-row button { position: relative; display: grid; place-items: center; flex: none; width: 16px; height: 16px; padding: 0; border: 1px solid transparent; background: none; cursor: pointer; }
.moon-row button.tagged::after { content: ""; position: absolute; right: 1px; top: 1px; width: 4px; height: 4px; background: var(--accent-hi); }
.moon-row button :deep(.icon) { width: 14px; height: 14px; }
</style>
