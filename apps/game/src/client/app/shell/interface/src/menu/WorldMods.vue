<script setup lang="ts">
import { computed, onMounted, ref, shallowRef } from "vue"
import { BridgeError, type RegistryListing, type WorldMod, type WorldMods } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { Button, Monogram, Notice } from "../kit/index.ts"
import { format } from "./library/format.ts"
import { useCache } from "../cache/index.ts"
import RegistryPicker from "./library/RegistryPicker.vue"

/**
 * A world's mods, in its settings: each at its pinned version, a newer one offered, and
 * out only if it hasn't put blocks in the world. Changes run terra, so they take seconds.
 */
const props = defineProps<{ path: string }>()
const emit = defineEmits<{ changed: [] }>()
const bridge = useBridge()
const cache = useCache()
const state = shallowRef<WorldMods | null>(null)
const latest = ref<Record<string, string>>({})
const busy = ref<string | null>(null) // what's being done, said
const problem = ref<string | null>(null)
const checkProblem = ref<string | null>(null)
const adding = ref(false)
const confirming = ref<string | null>(null) // a major update, asked about first
const names = computed(() => state.value?.mods.map((m) => m.name) ?? [])

const major = (v: string) => v.split(".")[0]
const newer = (m: WorldMod) => { const l = latest.value[m.name]; return l && l !== m.version ? l : null }

function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function change(doing: string, run: () => Promise<WorldMods>) {
    if (busy.value) return
    busy.value = doing
    problem.value = null
    try { state.value = await run(); emit("changed") }
    catch (err) { report(err) }
    finally { busy.value = null; confirming.value = null }
}
const add = (l: RegistryListing) => names.value.includes(l.name) ? undefined
    : change(`Adding ${format.bare(l.name)}…`, () => bridge.send("world.mods.add", { path: props.path, mods: [l.name] }))
const update = (m: WorldMod, to: string) => change(`Updating ${format.bare(m.name)}…`,
    () => bridge.send("world.mods.add", { path: props.path, mods: [`${m.name}@${to}`] }))
const remove = (m: WorldMod) => change(`Taking out ${format.bare(m.name)}…`,
    () => bridge.send("world.mods.remove", { path: props.path, name: m.name }))
function askUpdate(m: WorldMod, to: string) {
    if (major(to) !== major(m.version) && confirming.value !== m.name) confirming.value = m.name
    else void update(m, to)
}

async function checkUpdates(mods: readonly WorldMod[]) {
    const found: Record<string, string> = {}
    try {
        for (const m of mods) {
            const page = await cache.fetch("registry.mod", { name: m.name })
            if (page.latestVersion) found[m.name] = page.latestVersion
        }
        latest.value = found
    } catch (err) {
        checkProblem.value = `Couldn't check for updates: ${err instanceof Error ? err.message : String(err)}`
    }
}
onMounted(async () => {
    try { state.value = await bridge.send("world.mods", { path: props.path }) }
    catch (err) { report(err); return }
    void checkUpdates(state.value.mods)
})
</script>

<template>
    <section class="world-mods">
        <h3>Mods</h3>
        <p class="dim">Pinned to the versions shown. A mod that has put blocks in this world stays: taking it out would leave them unknown.</p>
        <p v-if="!state && !problem" class="dim">Reading this world's mods…</p>
        <div v-for="m in state?.mods ?? []" :key="m.name" class="row">
            <Monogram :name="m.name" :size="28" />
            <span class="copy">
                <strong>{{ format.bare(m.name) }}</strong>
                <small>{{ m.name }} · v{{ m.version }}<template v-if="m.official"> · the game's</template></small>
                <small v-if="confirming === m.name" class="warn">v{{ newer(m) }} is a new major version: its author says it may not fit existing worlds.</small>
            </span>
            <span class="acts">
                <Button v-if="newer(m)" size="sm" :variant="confirming === m.name ? 'danger' : 'primary'" :disabled="!!busy"
                    @click="askUpdate(m, newer(m)!)">{{ confirming === m.name ? "Update anyway" : `Update to v${newer(m)}` }}</Button>
                <span v-if="m.blocks.length" class="tag" :title="m.blocks.join(', ')">{{ m.blocks.length }} blocks here</span>
                <Button v-else size="sm" :disabled="!!busy" @click="remove(m)">Remove</Button>
            </span>
        </div>
        <p v-if="busy" class="dim">{{ busy }}</p>
        <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
        <Notice v-if="checkProblem" tone="warn">{{ checkProblem }}</Notice>
        <template v-if="state">
            <RegistryPicker v-if="adding" :chosen="names" :busy="!!busy" autofocus @pick="add" />
            <Button size="sm" @click="adding = !adding">{{ adding ? "Done adding" : "Add mods…" }}</Button>
        </template>
    </section>
</template>

<style scoped>
.world-mods { display: grid; justify-items: stretch; gap: 9px; padding-top: 14px; border-top: 1px solid var(--stone-4); }
.world-mods > .btn { justify-self: start; }
h3 { margin: 0; font: 11px var(--font-display); text-transform: uppercase; }
.dim { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.row { display: grid; grid-template-columns: 28px minmax(0, 1fr) auto; align-items: center; gap: 9px; padding: 5px 0; border-bottom: 1px solid var(--stone-3); }
.copy { display: grid; gap: 2px; min-width: 0; }
.copy strong { overflow: hidden; font: 11px var(--font-display); text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; }
.copy small { overflow: hidden; color: var(--ink-faint); font-size: 10px; text-overflow: ellipsis; white-space: nowrap; }
.copy .warn { color: #e0b33d; white-space: normal; }
.acts { display: flex; align-items: center; gap: 6px; }
.tag { color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; white-space: nowrap; }
</style>
