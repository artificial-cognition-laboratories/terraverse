<script setup lang="ts">
import { computed, ref } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { Button, Copier, Cover, Monogram } from "../../kit/index.ts"
import { format } from "./format.ts"

/** Discover's banner, as the website's (registry/featured.vue): one listing at a time, wide, with dots to step through the rest. */
const props = defineProps<{ listings: readonly RegistryListing[] }>()
defineEmits<{ open: [name: string] }>()
const at = ref(0)
const shown = computed(() => props.listings[Math.min(at.value, props.listings.length - 1)])
const install = Copier()
const command = computed(() => `terra install ${shown.value?.name ?? ""}`)
const STATE = { idle: "copy", copied: "copied", failed: "can't copy" } as const
</script>

<template>
    <section v-if="shown" class="featured">
        <Cover class="art" :seed="shown.name" :image="shown.banner" :width="96" />
        <div class="shade" />
        <div class="text">
            <span class="eyebrow">Featured{{ shown.official ? " · Official" : "" }}</span>
            <div class="name">
                <Monogram :name="shown.name" :image="shown.icon" :size="64" />
                <div>
                    <h3>{{ shown.title ?? format.bare(shown.name) }}</h3>
                    <span class="by">by {{ shown.owner }} · ★ {{ format.count(shown.stars) }}<template v-if="shown.installs !== null"> · ↓ {{ format.count(shown.installs) }}</template> · v{{ shown.latestVersion }}</span>
                </div>
            </div>
            <p v-if="shown.description">{{ shown.description }}</p>
            <div class="actions">
                <Button variant="primary" @click="$emit('open', shown.name)">View »</Button>
                <button type="button" class="install" @click="install.copy(command)">
                    <span class="prompt">$</span> {{ command }}
                    <span class="state">{{ STATE[install.state.value] }}</span>
                </button>
            </div>
        </div>
        <nav v-if="listings.length > 1" class="dots" aria-label="Featured">
            <button v-for="(l, i) in listings" :key="l.name" type="button" :class="{ on: i === at }" :aria-label="l.title ?? l.name" @click="at = i" />
        </nav>
    </section>
</template>

<style scoped>
.featured { position: relative; display: grid; min-height: 300px; overflow: hidden; border: 1px solid var(--glass-edge); border-radius: var(--radius); }
.art { position: absolute; inset: 0; }
.shade { position: absolute; inset: 0; background: linear-gradient(90deg, rgb(7 11 17 / 0.95) 0%, rgb(7 11 17 / 0.75) 45%, rgb(7 11 17 / 0) 80%); }
.text { position: relative; display: grid; align-content: end; gap: 1rem; max-width: 36rem; padding: 2rem; }
.eyebrow { color: var(--accent-hi); font: 0.65rem var(--font-display); text-transform: uppercase; }
.name { display: flex; gap: 1rem; align-items: center; min-width: 0; }
.name > div { display: grid; gap: 0.35rem; min-width: 0; }
h3 { margin: 0; overflow: hidden; font: 400 1.5rem var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.by { color: var(--ink-dim); font-size: 0.72rem; }
p { margin: 0; color: var(--ink-dim); font: 0.95rem/1.55 var(--font-sans); }
.actions { display: flex; flex-wrap: wrap; gap: 0.75rem; align-items: center; }
.install { height: 2.6rem; padding: 0 1rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.8); color: var(--ink-dim); font: 0.75rem var(--font); }
.install .prompt { color: var(--accent-hi); }
.install .state { margin-left: 0.8rem; color: var(--ink-faint); }
.install:hover { border-color: var(--accent-hi); color: var(--ink); }
.install:hover .state { color: var(--accent-hi); }
.dots { position: absolute; right: 1.2rem; bottom: 1.1rem; display: flex; gap: 0.4rem; }
.dots button { width: 0.7rem; height: 0.7rem; padding: 0; border: 0; background: var(--night-4); }
.dots button.on { background: var(--accent-hi); }
</style>
