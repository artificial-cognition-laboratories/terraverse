<script setup lang="ts">
import { computed } from "vue"
import type { RegistryListing } from "../../bridge/index.ts"
import { Button, Monogram, PageIntro } from "../../kit/index.ts"
import { format } from "../library/format.ts"
import RegistryPicker from "../library/RegistryPicker.vue"
import type { WorldDraftHandle } from "./draft.ts"
import { experience } from "./experiences.ts"

/** The new world's mods: what its experience brings, what you've added, and the registry to add from. */
const props = defineProps<{ draft: WorldDraftHandle }>()
const base = computed(() => experience(props.draft.mode))
function toggle(l: RegistryListing) {
    props.draft.mods = props.draft.mods.includes(l.name) ? props.draft.mods.filter((n) => n !== l.name) : [...props.draft.mods, l.name]
}
</script>

<template>
    <div class="mods-page">
        <PageIntro title="Mods" note="What this world runs. Anything you add from the registry downloads when you press Create, and stays at that version." />

        <section>
            <h3>In this world</h3>
            <div v-for="name in base.mods" :key="name" class="row">
                <Monogram :name="name" :size="30" />
                <span class="copy"><strong>{{ format.bare(name) }}</strong><small>{{ name }}</small></span>
                <span class="tag">With {{ base.title }}</span>
            </div>
            <div v-for="name in draft.mods" :key="name" class="row">
                <Monogram :name="name" :size="30" />
                <span class="copy"><strong>{{ format.bare(name) }}</strong><small>{{ name }}</small></span>
                <Button size="sm" @click="draft.mods = draft.mods.filter((n) => n !== name)">Remove</Button>
            </div>
            <p class="dim">Mods that shape the land or add blocks are fixed once the world is made. The rest can change later.</p>
        </section>

        <section>
            <h3>Add from the registry</h3>
            <RegistryPicker :chosen="draft.mods" :hidden="base.mods" autofocus @pick="toggle" />
        </section>
    </div>
</template>

<style scoped>
.mods-page { display: flex; flex-direction: column; gap: 18px; min-width: 0; }
section { display: grid; gap: 8px; min-width: 0; }
h3 { display: flex; align-items: center; gap: 8px; margin: 0; color: var(--accent-hi); font: 11px var(--font-display); text-transform: uppercase; }
.row { display: grid; grid-template-columns: 30px minmax(0, 1fr) auto; align-items: center; gap: 10px; padding: 6px 0; border-bottom: 1px solid var(--stone-3); }
.copy { display: grid; gap: 2px; min-width: 0; }
.copy strong { overflow: hidden; font: 11px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.copy small { overflow: hidden; color: var(--ink-faint); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
.tag { color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; white-space: nowrap; }
.dim { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
</style>
