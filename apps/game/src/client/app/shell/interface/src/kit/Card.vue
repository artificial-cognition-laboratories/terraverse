<script setup lang="ts">
import Cover from "./Cover.vue"
import Monogram from "./Monogram.vue"

/**
 * One thing in a grid you can open: its cover, mark, name, who made it and a line about it.
 * `meta` is the footer (stars, players, version); badges sit on the cover.
 */
withDefaults(defineProps<{
    title: string; seed: string; by?: string | null; description?: string | null
    image?: string | null; icon?: string | null; badges?: readonly string[]; dense?: boolean
}>(), { by: null, description: null, image: null, icon: null, badges: () => [], dense: false })
defineEmits<{ open: [] }>()
</script>

<template>
    <button class="card" :class="{ dense }" type="button" @click="$emit('open')">
        <div class="art">
            <Cover :seed="seed" :image="image" />
            <span v-if="badges.length" class="badges"><span v-for="b in badges" :key="b">{{ b }}</span></span>
        </div>
        <div class="text">
            <Monogram :name="seed" :image="icon" :size="dense ? 28 : 34" class="mark" />
            <div class="names">
                <strong>{{ title }}</strong>
                <span v-if="by" class="by">{{ by }}</span>
            </div>
        </div>
        <p v-if="description" class="description">{{ description }}</p>
        <div v-if="$slots.meta" class="meta"><slot name="meta" /></div>
    </button>
</template>

<style scoped>
.card { display: flex; flex-direction: column; min-width: 0; padding: 0; border: 1px solid var(--stone-3); background: none; color: var(--ink); text-align: left; }
.card:hover { border-color: var(--accent-lo); }
.card:hover .art { filter: brightness(1.12); }
.card:focus-visible { outline: var(--b) solid var(--accent); }
.art { position: relative; aspect-ratio: 16 / 9; border-bottom: 1px solid var(--stone-3); }
.art .cover { position: absolute; inset: 0; }
.badges { position: absolute; top: 6px; left: 6px; display: flex; gap: 4px; }
.badges span { padding: 3px 5px; background: rgb(12 14 16 / 0.86); border: 1px solid var(--accent-lo); color: var(--accent-hi); font: 8px var(--font-display); text-transform: uppercase; }
.text { display: flex; align-items: center; gap: 9px; min-width: 0; padding: 10px 10px 0; }
.mark { margin-top: -22px; background: var(--stone-0); }
.names { display: grid; gap: 2px; min-width: 0; }
strong { overflow: hidden; font: 11px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.by { overflow: hidden; color: var(--ink-faint); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
.description { display: -webkit-box; margin: 8px 10px 0; overflow: hidden; color: var(--ink-dim); font-size: 11px; line-height: 1.45; -webkit-line-clamp: 2; -webkit-box-orient: vertical; min-height: calc(11px * 1.45 * 2); }
.meta { display: flex; gap: 12px; margin-top: auto; padding: 10px; color: var(--ink-faint); font-size: 10px; white-space: nowrap; overflow: hidden; }
.dense .description { -webkit-line-clamp: 1; min-height: 0; }
</style>
