<script setup lang="ts">
import { ref, watch } from "vue"
import { BridgeError, type World } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Button, Field, Notice } from "../../kit/index.ts"
import { Copier } from "../../kit/copy.ts"
import WorldMods from "../WorldMods.vue"
import { play } from "./entries.ts"

/** A world's own page in the Play dock: its name, what it was made with, its mods, its code, and deleting it. */
const props = defineProps<{ world: World }>()
const emit = defineEmits<{ changed: []; removed: [] }>()
const bridge = useBridge()
const copier = Copier()
const naming = ref(props.world.name)
const confirming = ref(false)
const busy = ref(false)
const problem = ref<string | null>(null)
watch(() => props.world.path, () => { naming.value = props.world.name; confirming.value = false })

async function action(run: () => Promise<unknown>) {
    if (busy.value) return
    busy.value = true
    problem.value = null
    try { await run() }
    catch (err) {
        if (!(err instanceof BridgeError && err.refused)) console.error(err)
        problem.value = err instanceof Error ? err.message : String(err)
    }
    finally { busy.value = false }
}
const rename = () => action(async () => { await bridge.send("worlds.rename", { path: props.world.path, name: naming.value }); emit("changed") })
const pin = () => action(async () => { await bridge.send("worlds.pin", { path: props.world.path, pinned: !props.world.pinned }); emit("changed") })
const remove = () => action(async () => { await bridge.send("worlds.delete", { path: props.world.path }); emit("removed") })
</script>

<template>
    <div class="details">
        <h2>{{ world.name }}</h2>
        <p class="dim">{{ world.lastOpened ? `Last opened ${play.date(world.lastOpened)}` : "Ready for its first day" }}</p>
        <Notice v-if="problem" tone="danger">{{ problem }}</Notice>

        <section>
            <h3>Name</h3>
            <form class="row" @submit.prevent="rename()">
                <Field v-model="naming" label="World name" />
                <Button size="sm" type="submit" :disabled="busy || !naming.trim() || naming.trim() === world.name">Rename</Button>
            </form>
        </section>

        <section>
            <h3>This world</h3>
            <dl>
                <div v-if="world.mode"><dt>Mode</dt><dd>{{ world.mode }}</dd></div>
                <div v-if="world.rules.tf_difficulty"><dt>Difficulty</dt><dd>{{ world.rules.tf_difficulty }}</dd></div>
                <div v-if="play.planetSize(world)"><dt>Planet</dt><dd>{{ play.planetSize(world) }} block radius</dd></div>
                <div v-if="world.rules.tf_seasons"><dt>Seasons</dt><dd>{{ world.rules.tf_seasons === "true" ? "On" : "Off" }}</dd></div>
                <div v-if="world.rules.tf_weather"><dt>Weather</dt><dd>{{ world.rules.tf_weather }}</dd></div>
            </dl>
            <p class="dim">These were set when the world was made, and stay as they are.</p>
            <Button size="sm" :disabled="busy" @click="pin()">{{ world.pinned ? "Unpin world" : "Pin world" }}</Button>
        </section>

        <WorldMods :key="world.path" :path="world.path" @changed="emit('changed')" />

        <section v-if="world.code">
            <h3>Planet code</h3>
            <p class="dim">The same land and climate can be shared. Buildings and world rules stay in this save.</p>
            <code class="world-code">{{ world.code }}</code>
            <Button size="sm" @click="copier.copy(world.code!)">{{ copier.state.value === "copied" ? "Copied" : "Copy code" }}</Button>
            <Notice v-if="copier.state.value === 'failed'" tone="warn">Clipboard access failed. Select the code to copy it.</Notice>
        </section>

        <section class="danger">
            <h3>Delete</h3>
            <p class="dim">Removes this world and everything built in it from this device.</p>
            <template v-if="confirming">
                <p>Delete “{{ world.name }}” and its saved world?</p>
                <div class="row">
                    <Button size="sm" @click="confirming = false">Keep world</Button>
                    <Button size="sm" variant="danger" :disabled="busy" @click="remove()">Delete</Button>
                </div>
            </template>
            <Button v-else size="sm" variant="danger" @click="confirming = true">Delete world…</Button>
        </section>
    </div>
</template>

<style scoped>
.details { flex: 1; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: 1.2rem; padding: 1.4rem 1.4rem 2rem; }
h2 { margin: 0; font: 400 1.3rem var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
section { display: grid; justify-items: start; gap: 0.65rem; padding-top: 1rem; border-top: 1px solid var(--glass-edge); }
h3 { margin: 0; color: var(--ink-faint); font: 400 0.62rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
.dim { margin: 0; color: var(--ink-dim); font: 0.82rem/1.5 var(--font-sans); }
.row { display: flex; align-items: end; gap: 0.5rem; width: 100%; min-width: 0; }
.row :deep(.field), .row > :first-child:not(.btn) { flex: 1; min-width: 0; }
dl { display: grid; gap: 0.4rem; width: 100%; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 1rem; padding-bottom: 0.4rem; border-bottom: 1px solid rgb(231 237 244 / 0.05); font-size: 0.78rem; }
dt { color: var(--ink-dim); }
dd { margin: 0; text-align: right; font: 0.7rem var(--font-display); text-transform: uppercase; }
.world-code { max-width: 100%; overflow-wrap: anywhere; padding: 0.6rem 0.75rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); color: var(--accent-hi); font-size: 0.78rem; user-select: text; }
.danger p:not(.dim) { margin: 0; color: var(--danger); font-size: 0.82rem; }
</style>
