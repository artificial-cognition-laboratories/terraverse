<script setup lang="ts">
import { computed, ref } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { Button, Cover, Monogram } from "../../kit/index.ts"
import { format } from "./format.ts"

/** Discover's banner: one featured listing at a time, wide, with dots to step through the rest. */
const props = defineProps<{ listings: readonly RegistryListing[] }>()
defineEmits<{ open: [name: string] }>()
const at = ref(0)
const shown = computed(() => props.listings[Math.min(at.value, props.listings.length - 1)])
</script>

<template>
    <section v-if="shown" class="featured">
        <Cover class="art" :seed="shown.name" :image="shown.banner" :width="96" />
        <div class="shade" />
        <div class="text">
            <span class="eyebrow">Featured{{ shown.official ? " · Official" : "" }}</span>
            <div class="name">
                <Monogram :name="shown.name" :image="shown.icon" :size="52" />
                <div>
                    <h2>{{ shown.title ?? format.bare(shown.name) }}</h2>
                    <span class="by">by {{ shown.owner }} · ★ {{ format.count(shown.stars) }}<template v-if="shown.installs !== null"> · ↓ {{ format.count(shown.installs) }}</template></span>
                </div>
            </div>
            <p>{{ shown.description }}</p>
            <div><Button variant="primary" @click="$emit('open', shown.name)">View »</Button></div>
        </div>
        <nav v-if="listings.length > 1" class="dots" aria-label="Featured">
            <button v-for="(l, i) in listings" :key="l.name" type="button" :class="{ on: i === at }" :aria-label="l.title ?? l.name" @click="at = i" />
        </nav>
    </section>
</template>

<style scoped>
.featured { position: relative; display: grid; min-height: 230px; overflow: hidden; border: 1px solid var(--stone-3); }
.art { position: absolute; inset: 0; }
.shade { position: absolute; inset: 0; background: linear-gradient(90deg, rgb(12 14 16 / 0.94) 0%, rgb(12 14 16 / 0.75) 42%, rgb(12 14 16 / 0) 80%); }
.text { position: relative; display: grid; align-content: end; gap: 12px; max-width: 520px; padding: 24px 26px; }
.eyebrow { color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; }
.name { display: flex; gap: 14px; align-items: center; min-width: 0; }
.name > div { display: grid; gap: 5px; min-width: 0; }
h2 { margin: 0; overflow: hidden; font: 22px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.by { color: var(--ink-dim); font-size: 11px; }
p { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.dots { position: absolute; right: 16px; bottom: 14px; display: flex; gap: 6px; }
.dots button { width: 10px; height: 10px; padding: 0; border: var(--b) solid var(--edge); background: var(--stone-4); }
.dots button.on { background: var(--accent); }
</style>
