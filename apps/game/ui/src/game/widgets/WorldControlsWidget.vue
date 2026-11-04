<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import { useBridge } from "../../bridge/use"
import type { ControlIcon, ControlItem, ControlValue } from "../../bridge"
import { Button, Notice, Section, Segmented, Slider, Toggle } from "../../kit"
import IconButton from "../../kit/IconButton.vue"
import { useStore } from "../store"
import { widgetConfig } from "./config"

const props = defineProps<{ initialOpen?: boolean; initialPage?: string }>()
const bridge = useBridge()
const store = useStore()
const controls = computed(() => store.ui.world_controls)
const root = ref<HTMLElement | null>(null)
const expanded = ref(!!props.initialOpen)
const pageId = ref(props.initialPage ?? "time")
const problem = ref<string | null>(null)
const settings = ref<Record<string, ControlValue>>({})
const confirming = ref<string | null>(null)
const dragging = ref<Record<string, number>>({})

// Lua names a page's icon; a mod's own page gets the plain one
const ICONS: Record<ControlIcon, string> = {
    time: "M6 1h4v1h2v1h1v2h1v6h-1v2h-1v1h-2v1H6v-1H4v-1H3v-2H2V5h1V3h1V2h2zm1 3v5h4V7H9V4z",
    player: "M5 1h6v2h1v5h-1v1H5V8H4V3h1zm-1 10h8v1h2v3H2v-3h2z",
    travel: "M7 1h2v2h4l2 2-2 2H9v1h4v2H9v5H7v-5H3L1 8l2-2h4V5H3V3h4z",
    world: "M5 1h6v1h2v2h1v8h-1v2h-2v1H5v-1H3v-2H2V4h1V2h2zm0 3v2H4v2h3v2h2V8h2V6H9V4z",
    items: "M2 3h12v3H2zm1 4h10v7H3zm3 2v2h4V9z",
    tuning: "M1 3h14v2H1zm0 4h14v2H1zm0 4h14v2H1zM4 2h3v4H4zm6 4h3v4h-3zm-5 4h3v4H5z",
    view: "M5 3h6v1h2v1h1v1h1v4h-1v1h-1v1h-2v1H5v-1H3v-1H2v-1H1V6h1V5h1V4h2zm1 3v4h4V6z",
    other: "M2 2h12v12H2zm3 3v6h6V5z",
}
const page = computed(() => controls.value?.pages.find((p) => p.id === pageId.value) ?? controls.value?.pages[0] ?? null)

function valueOf(control: ControlItem): ControlValue | undefined {
    if (control.kind === "button") return undefined
    if (control.kind !== "fact" && control.setting) return settings.value[control.setting]
    return control.value
}
const toggled = (control: ControlItem) => valueOf(control) === true
function sliderValue(control: ControlItem & { kind: "slider" }) {
    const held = dragging.value[control.id]
    if (held !== undefined) return held
    const value = valueOf(control)
    if (typeof value !== "number") throw new Error(`world controls: ${control.id} has no number`)
    return value
}
function choiceValue(control: ControlItem & { kind: "choice" }) {
    const value = valueOf(control)
    if (typeof value === "boolean" || value === undefined) throw new Error(`world controls: ${control.id} has no choice`)
    return value
}
function format(control: ControlItem & { kind: "slider" }) {
    return (value: number) => {
        if (control.format === "clock") {
            const minutes = Math.round(value * 24 * 60) % (24 * 60)
            return `${String(Math.floor(minutes / 60)).padStart(2, "0")}:${String(minutes % 60).padStart(2, "0")}`
        }
        return `${Number(value.toFixed(2))}${control.unit ? ` ${control.unit}` : ""}`
    }
}
const clock = computed(() => {
    const hour = controls.value?.pages.flatMap((p) => p.sections).flatMap((s) => s.controls).find((c) => c.id === "time.hour")
    return hour?.kind === "slider" && typeof hour.value === "number" ? format(hour)(hour.value) : null
})

async function attempt(work: () => Promise<unknown>) {
    problem.value = null
    try { await work() } catch (error) { problem.value = error instanceof Error ? error.message : String(error) }
}
async function loadSettings() {
    await attempt(async () => { settings.value = await bridge.send("settings.get", {}) })
}
function change(control: ControlItem, value: ControlValue) {
    if (control.kind === "button" || control.kind === "fact") throw new Error(`world controls: ${control.id} holds no value`)
    const setting = control.setting
    if (setting) {
        settings.value = { ...settings.value, [setting]: value }
        return attempt(() => bridge.send("settings.set", { key: setting, value }))
    }
    return attempt(() => bridge.send("ui.send", { name: "controls.set", payload: { id: control.id, value } }))
}
function press(control: ControlItem & { kind: "button" }) {
    if (control.confirm && confirming.value !== control.id) {
        confirming.value = control.id
        return
    }
    confirming.value = null
    if (control.opens) void attempt(() => bridge.send("screen.open", { name: control.opens! }))
    else if (control.resets) void resetPage()
    else void attempt(() => bridge.send("ui.send", { name: "controls.set", payload: { id: control.id } }))
}
// Every setting on the page back to what Lua says is its default
async function resetPage() {
    for (const section of page.value?.sections ?? []) {
        for (const control of section.controls) {
            if (control.kind === "slider" && control.setting && control.default !== undefined)
                await change(control, control.default)
        }
    }
}
function drag(control: ControlItem & { kind: "slider" }, value: number) {
    dragging.value = { ...dragging.value, [control.id]: value }
}
function drop(control: ControlItem & { kind: "slider" }, value: number) {
    const { [control.id]: _, ...rest } = dragging.value
    dragging.value = rest
    void change(control, value)
}

function keydown(event: KeyboardEvent) {
    if (expanded.value && event.key === "Escape") {
        event.preventDefault()
        event.stopPropagation()
        expanded.value = false
        root.value?.querySelector<HTMLButtonElement>(".world-trigger")?.focus()
    }
}
onMounted(() => {
    document.addEventListener("keydown", keydown)
})
onUnmounted(() => {
    document.removeEventListener("keydown", keydown)
})
watch(controls, (value) => { if (!value) expanded.value = false })
watch(expanded, (open) => { if (open) void loadSettings() }, { immediate: true })
watch(pageId, () => { confirming.value = null })
</script>

<template>
    <div v-if="widgetConfig.ready && widgetConfig.worldControls && controls" ref="root" class="world-controls"
        :class="`corner-${widgetConfig.worldControlsCorner}`">
        <IconButton class="world-trigger" label="World controls" :on="expanded"
            aria-controls="world-controls-panel" :aria-expanded="expanded" @click="expanded = !expanded">
            <svg viewBox="0 0 16 16" width="25" height="25" shape-rendering="crispEdges" aria-hidden="true">
                <path fill="currentColor" d="M1 3h14v2H1zm0 4h14v2H1zm0 4h14v2H1z" />
                <path fill="var(--accent-hi)" d="M4 2h3v4H4zm6 4h3v4h-3zm-5 4h3v4H5z" />
            </svg>
            <span class="mode-badge">{{ controls.mode === 'creative' ? 'C' : 'S' }}</span>
        </IconButton>

        <!-- On the body, to sit over the pause menu. The page only hears the mouse where it has
             painted, so the catcher is faintly painted: clicking off lands on it, not the world. -->
        <Teleport to="body">
        <div v-if="expanded" class="catcher" @pointerdown="expanded = false" />
        <section v-if="expanded" id="world-controls-panel" class="dropdown" :class="`corner-${widgetConfig.worldControlsCorner}`" aria-label="World controls">
            <header class="identity">
                <div class="avatar" aria-hidden="true">{{ controls.mode === 'creative' ? 'C' : 'S' }}</div>
                <div class="identity-text">
                    <strong>World controls</strong>
                    <span class="handle"><span class="live" />{{ controls.mode === 'creative' ? 'Creative' : 'Survival' }}<template v-if="clock"> · {{ clock }}</template></span>
                </div>
            </header>
            <div class="hub">
                <nav class="rail" aria-label="World control pages">
                    <button v-for="p in controls.pages" :key="p.id" :class="{ selected: p.id === page?.id }" :aria-label="p.title"
                        :aria-current="p.id === page?.id ? 'page' : undefined" @click="pageId = p.id">
                        <svg viewBox="0 0 16 16" shape-rendering="crispEdges" aria-hidden="true"><path fill="currentColor" fill-rule="evenodd" :d="ICONS[p.icon] ?? ICONS.other" /></svg>
                        <span>{{ p.title }}</span>
                    </button>
                </nav>
                <div class="content">
                    <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
                    <template v-if="page">
                        <Section v-for="section in page.sections" :key="section.title" :title="section.title" :hint="section.hint">
                            <div class="buttons" v-if="section.controls.some((c) => c.kind === 'button')">
                                <template v-for="control in section.controls" :key="control.id">
                                    <Button v-if="control.kind === 'button'" size="sm" :variant="control.tone === 'danger' ? 'danger' : undefined" @click="press(control)">
                                        {{ confirming === control.id ? 'Sure?' : control.label }}
                                    </Button>
                                </template>
                            </div>
                            <template v-for="control in section.controls" :key="control.id">
                                <div v-if="control.kind === 'fact'" class="fact"><span>{{ control.label }}</span><strong>{{ control.value }}</strong></div>
                                <template v-else-if="control.kind === 'toggle'">
                                    <Toggle :model-value="toggled(control)" :label="control.label" @update:model-value="change(control, $event)" />
                                    <p v-if="control.note" class="note">{{ control.note }}</p>
                                </template>
                                <Slider v-else-if="control.kind === 'slider' && valueOf(control) !== undefined" :model-value="sliderValue(control)" :label="control.label"
                                    :min="control.min" :max="control.max" :step="control.step" :format="format(control)"
                                    @update:model-value="drag(control, $event)" @change="drop(control, $event)" />
                                <div v-else-if="control.kind === 'choice' && valueOf(control) !== undefined" class="choice">
                                    <span>{{ control.label }}</span>
                                    <Segmented :model-value="choiceValue(control)" :options="control.options" @update:model-value="$event !== null && change(control, $event)" />
                                </div>
                            </template>
                        </Section>
                    </template>
                </div>
            </div>
        </section>
        </Teleport>
    </div>
</template>

<style scoped>
.world-controls { position: fixed; z-index: 6; max-width: calc(100vw - 40px); }
.corner-0 { top: 20px; left: 20px; }
.corner-1 { top: 20px; right: 80px; }
.corner-2 { bottom: 20px; left: 20px; }
.corner-3 { bottom: 20px; right: 20px; }
.mode-badge { position: absolute; right: 2px; bottom: 2px; width: 15px; height: 15px; background: var(--accent-lo); border: 1px solid var(--edge); color: var(--ink); font: 10px/13px var(--font-display); text-align: center; }
.catcher { position: fixed; inset: 0; z-index: 20; background: rgb(0 0 0 / 0.01); }
.dropdown {
    position: fixed; z-index: 21; width: min(440px, calc(100vw - 40px)); height: min(540px, calc(100vh - 96px));
    display: flex; flex-direction: column; overflow: hidden;
    border: var(--b) solid var(--edge); background: var(--stone-1);
    box-shadow: var(--raised), 8px 8px 0 rgb(0 0 0 / 0.35);
}
/* clear of the button, on its side of the screen */
.dropdown.corner-0, .dropdown.corner-1 { top: 78px; bottom: auto; }
.dropdown.corner-2, .dropdown.corner-3 { top: auto; bottom: 78px; }
.identity { flex: none; display: flex; align-items: center; gap: 10px; min-height: 54px; padding: 8px 10px; background: var(--stone-2); border-bottom: var(--b) solid var(--edge); box-shadow: var(--raised); }
.avatar { flex: none; display: grid; place-items: center; width: 34px; height: 34px; background: var(--stone-3); border: var(--b) solid var(--edge); box-shadow: var(--raised); font: 15px var(--font-display); color: var(--accent-hi); }
.identity-text { min-width: 0; display: flex; flex-direction: column; justify-content: center; line-height: 1.3; }
.identity-text strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font: 12px var(--font-display); text-transform: uppercase; }
.handle { color: var(--ink-dim); font-size: 10px; overflow: hidden; text-overflow: ellipsis; }
.live { display: inline-block; width: 7px; height: 7px; margin-right: 5px; background: var(--accent-hi); border: 1px solid var(--edge); }
.hub { display: grid; grid-template-columns: 62px minmax(0, 1fr); min-height: 0; flex: 1; }
.rail { display: flex; flex-direction: column; gap: 5px; min-height: 0; padding: 8px 5px; overflow-y: auto; border-right: var(--b) solid var(--edge); background: var(--stone-2); }
.rail button { flex: none; display: flex; flex-direction: column; align-items: center; gap: 3px; min-width: 0; padding: 8px 2px 7px; border: var(--b) solid transparent; background: none; color: var(--ink-faint); font: 8px var(--font-display); text-transform: uppercase; }
.rail button:hover { color: var(--ink); background: var(--stone-3); }
.rail button.selected { border-color: var(--edge); background: var(--accent-lo); box-shadow: var(--sunk); color: var(--ink); }
.rail svg { width: 18px; height: 18px; }
.rail button.selected svg { color: var(--accent-hi); }
.content { min-width: 0; min-height: 0; overflow-y: auto; padding: 12px; display: flex; flex-direction: column; gap: 18px; }
.buttons { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px; margin-bottom: 6px; }
.fact { display: flex; justify-content: space-between; gap: 12px; padding: 7px 0; border-bottom: 1px solid var(--stone-3); font-size: 12px; }
.fact strong { font: 10px var(--font-display); text-transform: uppercase; text-align: right; }
.choice { display: flex; flex-direction: column; gap: 5px; font-size: 12px; }
.note { margin: 0; color: var(--ink-dim); font-size: 10px; }
</style>
