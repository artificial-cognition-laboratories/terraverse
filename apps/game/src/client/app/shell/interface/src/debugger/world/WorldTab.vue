<script setup lang="ts">
/**
 * The game's systems as they run (shell/host/inspectors.h): one view each,
 * picked along the top. Each shows its controls (what to draw, what to
 * stop or poke), then its numbers now. Outside a world, nothing's inspected.
 */
import { computed, ref, watch } from "vue"
import Details from "../kit/Details.vue"
import SubTabs from "../kit/SubTabs.vue"
import InspectorControls from "./InspectorControls.vue"
import type { WorldStoreHandle } from "./store.ts"

const props = defineProps<{ world: WorldStoreHandle }>()
const names = computed(() => Object.keys(props.world.inspectors.value).sort())
const views = computed(() => names.value.map((n) => ({ id: n, label: props.world.inspectors.value[n]?.title ?? n })))
const view = ref<string | null>(null)
watch(names, (now) => {
    if (view.value === null || !now.includes(view.value)) view.value = now[0] ?? null
}, { immediate: true })
const shown = computed(() => (view.value === null ? null : props.world.inspectors.value[view.value] ?? null))
const sections = computed(() => shown.value
    ? [{ title: "Now", about: shown.value.about, rows: shown.value.stats.map((s) => ({ name: s.label, value: s.value, mono: true })) }]
    : [])
const act = (control: string, value: string | boolean | null) => {
    if (view.value !== null) void props.world.act(view.value, control, value)
}
</script>

<template>
    <div class="world">
        <template v-if="shown && view !== null">
            <div class="views">
                <SubTabs :model-value="view" :tabs="views" label="Inspected systems" @update:model-value="(id: string) => (view = id)" />
            </div>
            <div class="body">
                <InspectorControls :controls="shown.controls" @act="act" />
                <Details :sections="sections" />
            </div>
        </template>
        <p v-else class="none">Nothing's inspected: the game's systems show here while you're in a world.</p>
    </div>
</template>

<style scoped>
.world { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.views { display: flex; align-items: center; flex: none; height: 30px; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
.body { flex: 1; min-height: 0; overflow-y: auto; }
.none { margin: 24px 14px; font-size: 12px; color: var(--dt-dim); }
</style>
