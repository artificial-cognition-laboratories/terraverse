<script setup lang="ts">
import { nextTick, onMounted, ref, watch } from "vue"
import { BridgeError } from "../bridge"
import { useBridge } from "../bridge/use"
import { Button, Notice } from "../kit"
import { BINDING_GROUPS, BINDINGS, bindingLabel, bindingScancode, scancodeFor } from "./bindings"

const props = defineProps<{ focusKey?: string | null }>()
const bridge = useBridge()
const root = ref<HTMLElement | null>(null)
const values = ref<Record<string, string> | null>(null)
const recorder = ref<HTMLInputElement | null>(null)
const recording = ref<string | null>(null)
const pending = ref<{ key: string; scancode: number; other: string } | null>(null)
const resetConfirm = ref(false)
const busy = ref(false)
const problem = ref<string | null>(null)

function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function refresh() {
    try { values.value = await bridge.send("settings.bindings.get", {}) }
    catch (err) { report(err) }
}
async function start(key: string) {
    if (busy.value) return
    pending.value = null
    recording.value = key
    await nextTick()
    recorder.value?.focus()
}
async function commit(key: string, scancode: number, replace: boolean) {
    busy.value = true
    problem.value = null
    try {
        await bridge.send("settings.bindings.set", { key, scancode, replace })
        pending.value = null
        await refresh()
    } catch (err) { report(err) }
    finally { busy.value = false }
}
function captured(event: KeyboardEvent) {
    event.preventDefault()
    event.stopPropagation()
    const key = recording.value
    if (!key) return
    if (event.code === "Escape") { recording.value = null; recorder.value?.blur(); return }
    const scancode = scancodeFor(event.code)
    if (scancode === null) {
        problem.value = `${event.code || "That key"} cannot be assigned.`
        return
    }
    recording.value = null
    recorder.value?.blur()
    if (bindingScancode(values.value?.[key] ?? "") === scancode) return
    const other = Object.entries(values.value ?? {}).find(([name, value]) =>
        name !== key && bindingScancode(value) === scancode)?.[0]
    if (other) pending.value = { key, scancode, other }
    else commit(key, scancode, false)
}
async function reset() {
    busy.value = true
    problem.value = null
    try {
        await bridge.send("settings.bindings.reset", {})
        resetConfirm.value = false
        pending.value = null
        await refresh()
    } catch (err) { report(err) }
    finally { busy.value = false }
}
function name(key: string) { return BINDINGS.find((b) => b.key === key)?.label ?? key.replace(/^keymap_/, "").replaceAll("_", " ") }
onMounted(refresh)
watch([() => props.focusKey, values], async ([key]) => {
    if (!key) return
    await nextTick()
    root.value?.querySelector<HTMLElement>(`[data-binding="${key}"]`)?.scrollIntoView({ block: "center" })
}, { immediate: true })
</script>

<template>
    <section ref="root" class="bindings">
        <input ref="recorder" class="recorder" aria-label="Press a key to assign it" readonly
            @keydown="captured" @blur="recording = null">
        <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
        <Notice v-if="pending" tone="warn">
            {{ bindingLabel(`SYSTEM_SCANCODE_${pending.scancode}`) }} is assigned to {{ name(pending.other) }}.
            Replace that shortcut?
            <span class="confirm-actions">
                <Button size="sm" @click="pending = null">Cancel</Button>
                <Button size="sm" variant="primary" :disabled="busy" @click="commit(pending!.key, pending!.scancode, true)">Replace</Button>
            </span>
        </Notice>
        <p class="hint">Choose an action, then press a key. Existing mouse and gamepad controls stay assigned. Esc cancels recording.</p>
        <p v-if="!values" class="hint">Reading controls…</p>
        <template v-else>
            <section v-for="group in BINDING_GROUPS" :key="group.title" class="group">
                <h3>{{ group.title }}</h3>
                <div v-for="[key, label] in group.bindings" :key="key" class="binding-row" :data-binding="key" :class="{ focused: focusKey === key }">
                    <span>{{ label }}</span>
                    <Button size="sm" :disabled="busy" @click="start(key)">
                        {{ recording === key ? "Press a key…" : bindingLabel(values[key] ?? "") }}
                    </Button>
                </div>
            </section>
            <div class="reset">
                <template v-if="resetConfirm">
                    <span>Restore all default controls?</span>
                    <Button size="sm" @click="resetConfirm = false">Cancel</Button>
                    <Button size="sm" variant="danger" :disabled="busy" @click="reset">Restore</Button>
                </template>
                <Button v-else size="sm" @click="resetConfirm = true">Reset all controls</Button>
            </div>
        </template>
    </section>
</template>

<style scoped>
/* the settings' rows (SettingsPanel): sans labels, hairlines between, the found one lit */
.bindings { display: grid; gap: 1.25rem; }
.recorder { position: absolute; width: 1px; height: 1px; opacity: 0; pointer-events: none; }
.hint { max-width: 68ch; margin: 0; color: var(--ink-faint); font: 0.75rem/1.5 var(--font-sans); }
.group { display: grid; }
h3 { margin: 0; padding: 0 0 0.5rem; border-bottom: 1px solid var(--glass-edge); color: var(--ink-faint); font: 400 0.62rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
.binding-row { display: flex; align-items: center; justify-content: space-between; gap: 1rem; padding: 0.45rem 0.75rem; border-bottom: 1px solid var(--glass-edge); color: var(--ink-dim); font: 0.88rem var(--font-sans); }
.binding-row:hover { color: var(--ink); }
.binding-row.focused { color: var(--ink); background: rgb(99 163 233 / 0.05); }
.binding-row :deep(.btn) { min-width: 132px; }
.reset, .confirm-actions { display: flex; align-items: center; gap: 0.5rem; flex-wrap: wrap; }
.reset { padding-top: 0.5rem; color: var(--ink-dim); font: 0.75rem var(--font-sans); }
.confirm-actions { margin-top: 0.6rem; }
</style>
