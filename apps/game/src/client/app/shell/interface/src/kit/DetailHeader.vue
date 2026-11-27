<script setup lang="ts">
import { Copier } from "./copy.ts"
import Cover from "./Cover.vue"
import Monogram from "./Monogram.vue"

/**
 * The top of a thing's own page, as the website's registry detail (registry/detail/header.vue):
 * its banner, with `address` on it (a click copies `copies`), its mark, name and badges, what you do
 * with it at the end of that line (the `actions` slot), who it's by (`by`), and its words. A mod, a
 * server and a player all open with one.
 */
withDefaults(defineProps<{
    seed: string; title: string; mark?: string; banner?: string | null; icon?: string | null
    address?: string | null; copies?: string | null; description?: string | null
}>(), { mark: undefined, banner: null, icon: null, address: null, copies: null, description: null })
const copier = Copier()
const STATE = { idle: "copy", copied: "copied", failed: "can't copy" } as const
</script>

<template>
    <header class="detail-header">
        <div class="banner">
            <Cover class="art" :seed="seed" :image="banner" :width="120" />
            <button v-if="address" type="button" class="address" :title="copies ?? address" @click="copier.copy(copies ?? address)">
                <code>{{ address }}</code>
                <span class="state">{{ STATE[copier.state.value] }}</span>
            </button>
        </div>
        <div class="title">
            <Monogram :name="mark ?? seed" :image="icon" :size="64" />
            <div class="names">
                <div class="line">
                    <h1>{{ title }}</h1>
                    <slot name="badges" />
                    <div class="end"><slot name="actions" /></div>
                </div>
                <span v-if="$slots.by" class="by"><slot name="by" /></span>
            </div>
        </div>
        <p v-if="description" class="description">{{ description }}</p>
        <slot />
    </header>
</template>

<style scoped>
.detail-header { display: grid; gap: 1.25rem; }
.banner { position: relative; height: 9rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); }
.art { position: absolute; inset: 0; }
/* the address, copied: quiet until it's wanted */
.address { position: absolute; top: 0.75rem; right: 0.75rem; display: flex; gap: 0.6rem; align-items: center; padding: 0.35rem 0.6rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.55); backdrop-filter: blur(6px); color: var(--ink-dim); font: 0.72rem var(--font); }
.address code { font: inherit; }
.address .state { display: none; color: var(--ink-faint); }
.address:hover { color: var(--ink); border-color: var(--edge-lit-hi); }
.address:hover .state { display: inline; }
.title { position: relative; display: flex; gap: 1.1rem; align-items: center; margin-top: -3.25rem; padding: 0 1.25rem; }
.names { flex: 1; display: grid; gap: 0.35rem; min-width: 0; padding-top: 2.5rem; }
.line { display: flex; flex-wrap: wrap; gap: 0.75rem; align-items: center; }
h1 { margin: 0; color: var(--ink); font: 400 clamp(1.4rem, 3vw, 2rem) var(--font-display); text-transform: uppercase; }
.line :deep(.badge) { padding: 0.2rem 0.45rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); color: var(--ink-dim); font: 0.68rem var(--font); }
.line :deep(.badge.lit) { border-color: var(--edge-lit-hi); color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; }
/* the actions sit at the far end of the title line, level with the name */
.end { display: flex; gap: 0.6rem; align-items: center; margin-left: auto; }
.by { color: var(--ink-faint); font-size: 0.78rem; }
.by :deep(b) { font-weight: 400; color: var(--accent-hi); }
.description { max-width: 44rem; margin: 0; color: var(--ink-dim); font: 1.05rem/1.6 var(--font-sans); }
</style>
