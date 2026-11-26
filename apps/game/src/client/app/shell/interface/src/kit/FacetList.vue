<script setup lang="ts">
import PixelIcon from "../game/PixelIcon.vue"
import type { Icon } from "./icons.ts"

/** A filter's options in a side card, any number picked: a box, an icon if it has one, its name and how many it would show. */
defineProps<{ title: string; options: readonly { id: string; label: string; count?: number | null; icon?: Icon | null }[] }>()
const model = defineModel<string[]>({ required: true })
function flip(id: string) {
    model.value = model.value.includes(id) ? model.value.filter((v) => v !== id) : [...model.value, id]
}
</script>

<template>
    <section class="facets">
        <h4>{{ title }}</h4>
        <button v-for="o in options" :key="o.id" type="button" role="checkbox" :aria-checked="model.includes(o.id)"
            :class="{ on: model.includes(o.id) }" @click="flip(o.id)">
            <span class="box" aria-hidden="true" />
            <PixelIcon v-if="o.icon" :rows="o.icon.rows" :palette="o.icon.palette" style="--u: 1.6px" />
            <span class="label">{{ o.label }}</span>
            <span v-if="o.count != null" class="count">{{ o.count }}</span>
        </button>
    </section>
</template>

<style scoped>
.facets { display: grid; gap: 0.15rem; min-width: 0; }
h4 { margin: 0 0 0.6rem; color: var(--ink-faint); font: 400 0.62rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
button { display: flex; gap: 0.7rem; align-items: center; width: 100%; padding: 0.45rem 0.25rem; border: 0; background: none; text-align: left; }
.box { flex: none; width: 0.8rem; height: 0.8rem; border: 1px solid var(--night-4); border-radius: var(--radius); }
.label { flex: 1; min-width: 0; overflow: hidden; color: var(--ink-dim); font: 0.88rem var(--font-sans); text-overflow: ellipsis; white-space: nowrap; }
.count { color: var(--ink-faint); font-size: 0.7rem; }
button:hover .label, .on .label { color: var(--ink); }
.on .box { background: var(--accent-lo); border-color: var(--accent-hi); box-shadow: inset 0 0 0 2px var(--night-sky); }
</style>
