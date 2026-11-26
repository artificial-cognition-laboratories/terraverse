<script setup lang="ts">
import Cover from "./Cover.vue"
import Monogram from "./Monogram.vue"

/**
 * One thing you can open, as the website's registry draws it (apps/site registry/card.vue,
 * row.vue): its cover, mark, name, who made it, a line or two, and `meta` along the foot.
 * `row` lays the same card on its side, for a list. Badges sit on the cover. `on`: the chosen one,
 * when cards are a choice.
 */
withDefaults(defineProps<{
    title: string; seed: string; by?: string | null; description?: string | null
    image?: string | null; icon?: string | null; badges?: readonly string[]; dense?: boolean; row?: boolean; on?: boolean
}>(), { by: null, description: null, image: null, icon: null, badges: () => [], dense: false, row: false, on: false })
defineEmits<{ open: [] }>()
</script>

<template>
    <button class="card" :class="{ dense, row, on }" type="button" :aria-pressed="on || undefined" @click="$emit('open')">
        <div class="art">
            <Cover :seed="seed" :image="image" :width="row ? 24 : 32" />
            <span v-if="badges.length" class="badges"><span v-for="b in badges" :key="b">{{ b }}</span></span>
        </div>
        <div class="body">
            <div class="head">
                <Monogram :name="seed" :image="icon" :size="row ? 35 : 45" />
                <div class="names">
                    <strong>{{ title }}</strong>
                    <span v-if="by" class="by">{{ by }}</span>
                </div>
            </div>
            <p v-if="description" class="description">{{ description }}</p>
            <div v-if="$slots.meta" class="meta"><slot name="meta" /></div>
        </div>
    </button>
</template>

<style scoped>
.card { display: flex; flex-direction: column; min-width: 0; padding: 0; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); color: var(--ink); text-align: left; transition: border-color 0.1s; }
.card:hover { border-color: var(--edge-lit); }
.card:hover strong { color: var(--accent-hi); }
.card:focus-visible { outline: 1px solid var(--accent-hi); outline-offset: 2px; }
.art { position: relative; aspect-ratio: 16 / 9; border-bottom: 1px solid var(--glass-edge); }
.art .cover { position: absolute; inset: 0; }
.badges { position: absolute; top: 0.6rem; left: 0.6rem; display: flex; gap: 0.3rem; }
.badges span { padding: 0.2rem 0.4rem; background: rgb(7 11 17 / 0.85); border: 1px solid rgb(99 163 233 / 0.55); border-radius: var(--radius); color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; }
.body { display: flex; flex-direction: column; flex: 1; min-width: 0; }
.head { position: relative; display: flex; gap: 0.75rem; align-items: center; min-width: 0; margin: -1.3rem 0.9rem 0; }
.names { display: grid; min-width: 0; padding-top: 1.3rem; }
strong { overflow: hidden; font: 400 0.78rem var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; transition: color 0.1s; }
.by { overflow: hidden; color: var(--ink-faint); font-size: 0.68rem; text-overflow: ellipsis; white-space: nowrap; }
.description { flex: 1; display: -webkit-box; margin: 0.8rem 0.9rem 0; overflow: hidden; color: var(--ink-dim); font: 0.82rem/1.5 var(--font-sans); -webkit-line-clamp: 2; -webkit-box-orient: vertical; }
.meta { display: flex; gap: 0.9rem; align-items: center; padding: 0.9rem; color: var(--ink-faint); font-size: 0.68rem; white-space: nowrap; overflow: hidden; }
.meta :deep(.tag) { margin-left: auto; padding: 0.1rem 0.4rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); }
.card.on { border-color: var(--accent-hi); box-shadow: inset 0 0 0 1px var(--accent-hi); }
.card.on strong { color: var(--accent-hi); }
.dense .description { -webkit-line-clamp: 1; }

/* on its side: the picture down the left, the words beside it */
.row { flex-direction: row; }
.row .art { flex: none; width: 11rem; aspect-ratio: auto; border-bottom: 0; border-right: 1px solid var(--glass-edge); }
.row .badges span { font-size: 0.5rem; }
.row .body { gap: 0.6rem; padding: 1rem 1.2rem; }
.row .head { margin: 0; }
.row .names { padding-top: 0; }
.row strong { font-size: 0.8rem; }
.row .description { flex: none; display: block; margin: 0; font-size: 0.88rem; text-overflow: ellipsis; white-space: nowrap; }
.row .meta { flex-wrap: wrap; gap: 0.4rem 1rem; padding: 0; }
</style>
