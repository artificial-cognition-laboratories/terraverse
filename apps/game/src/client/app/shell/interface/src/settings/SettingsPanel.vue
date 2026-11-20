<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from "vue"
import { BridgeError } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { Button, Choice, ListEmpty, ListGroup, ListItem, Notice, Segmented, Slider, Toggle, WorkbenchPanel } from "../kit/index.ts"
import { CATEGORIES, GRAPHICS_PRESETS, type Page, type Category, type Option, type SettingValue } from "./options.ts"
import { BINDINGS } from "./bindings.ts"
import Keybindings from "./Keybindings.vue"
import { publishAudioLevels } from "./audio.ts"
import { applyWidgetSettings } from "../game/widgets/config.ts"

const bridge = useBridge()
const APPLIES = { rejoin: "Applies when you next enter a world.", restart: "Applies when you restart the game." }
defineProps<{ closeLabel?: string }>()
const emit = defineEmits<{ close: [] }>()
const values = ref<Record<string, SettingValue> | null>(null)
const categoryId = ref("graphics")
const pageId = ref("presets")
const query = defineModel<string>("query", { required: true })
const selectedKey = ref<string | null>(null)
const problem = ref<string | null>(null)
const busy = ref(false)
const panel = ref<{ $el: HTMLElement } | null>(null)

const category = computed(() => CATEGORIES.find((c) => c.id === categoryId.value)!)
const page = computed(() => category.value.pages.find((p) => p.id === pageId.value) ?? category.value.pages[0]!)
const panelSection = computed(() => query.value.trim() ? "Search" : category.value.title)
const railChoice = computed({
    get: () => categoryId.value,
    set: (id: string) => { query.value = ""; categoryId.value = id },
})
const STEEL = "#c9d4dc", SHADE = "#6f7f8a", SKY = "#40b8ec", SUN = "#ffb040", EMBER = "#e0603d", GRASS = "#3c6e46"
const RAIL: { id: string; label: string; rows: string[]; palette: Record<string, string> }[] = [
    { id: "display", label: "Display", palette: { G: STEEL, D: SHADE, L: SKY, O: SUN },
        rows: ["GGGGGGGGG", "GLLLLLOOG", "GLLLLLOOG", "GLLLLLLLG", "GLLLLLLLG", "GGGGGGGGG", "...DDD...", "...DDD...", "..GGGGG.."] },
    { id: "graphics", label: "Graphics", palette: { O: SUN, W: STEEL, G: SHADE, T: GRASS },
        rows: ["......OO.", "......OO.", "...W.....", "..WWW..G.", "..GGG.GGG", ".GGGGGGGG", ".GGGGGGGG", "GGGGGGGGG", "TTTTTTTTT"] },
    { id: "controls", label: "Controls", palette: { G: STEEL, D: SHADE, R: EMBER },
        rows: [".........", ".GGG.GGG.", "GGGGGGGGG", "GGDGGGRGG", "GDDDGRGRG", "GGDGGGRGG", "GGGGGGGGG", "GGG...GGG", "GG.....GG"] },
    { id: "audio", label: "Audio", palette: { G: STEEL, L: SKY },
        rows: ["....G....", "...GG..L.", "..GGG...L", "GGGGG.L.L", "GGGGG.L.L", "GGGGG.L.L", "..GGG...L", "...GG..L.", "....G...."] },
    { id: "interface", label: "Interface", palette: { G: STEEL, R: SUN, S: SHADE, W: "#ffffff" },
        rows: ["...GGG...", ".GG.R.GG.", ".G..R..G.", "G...R...G", "G...W...G", "G...S...G", ".G..S..G.", ".GG.S.GG.", "...GGG..."] },
]
const pageGroups = computed(() => {
    const groups: { title: string; pages: Page[] }[] = []
    for (const item of category.value.pages) {
        let group = groups.find((g) => g.title === item.group)
        if (!group) { group = { title: item.group, pages: [] }; groups.push(group) }
        group.pages.push(item)
    }
    return groups
})
type SearchMatch = { key: string; label: string; category: Category; page: Page }
const matches = computed(() => {
    const words = query.value.trim().toLowerCase().split(/\s+/).filter(Boolean)
    if (!words.length) return []
    return CATEGORIES.flatMap((c) => c.pages.flatMap((p): SearchMatch[] => [
        ...p.options.filter((o) => words.every((word) =>
            `${c.title} ${p.title} ${o.label} ${o.note} ${o.terms ?? ""}`.toLowerCase().includes(word)))
            .map((o) => ({ key: o.key, label: o.label, category: c, page: p })),
        ...(p.bindings ? BINDINGS.filter((b) => words.every((word) =>
            `${c.title} ${p.title} ${b.label} ${b.group}`.toLowerCase().includes(word)))
            .map((b) => ({ key: b.key, label: b.label, category: c, page: p })) : []),
    ]))
})
const focusBinding = computed(() => page.value.bindings ? selectedKey.value : null)
const preset = computed(() => {
    if (!values.value) return null
    return GRAPHICS_PRESETS.find((p) =>
        Object.entries(p.values).every(([key, value]) => values.value?.[key] === value))?.id ?? null
})
const presetOptions = GRAPHICS_PRESETS.map(({ id, label, note }) => ({ id, label, note }))
const chosenPreset = computed({
    get: () => preset.value,
    set: (id: string | null) => { if (id) applyPreset(id) },
})

watch(categoryId, () => { pageId.value = category.value.pages[0]!.id }, { flush: "sync" })
watch(pageId, () => {
    selectedKey.value = null
    void nextTick(() => panel.value?.$el.querySelector<HTMLElement>(".workbench-content")?.scrollTo({ top: 0 }))
}, { flush: "sync" })

function numberValue(key: string) {
    const value = values.value?.[key]
    if (typeof value !== "number") throw new Error(`setting ${key} is not a number`)
    return value
}
function flagValue(key: string) {
    const value = values.value?.[key]
    if (typeof value !== "boolean") throw new Error(`setting ${key} is not a flag`)
    return value
}
function choiceValue(key: string) {
    const value = values.value?.[key]
    if (typeof value !== "string") throw new Error(`setting ${key} is not a choice`)
    return value
}
function available(option: Option) {
    return !option.requires || values.value?.[option.requires.key] === option.requires.value
}
function stage(key: string, value: number) {
    values.value = { ...values.value, [key]: value }
}
function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function save(key: string, value: SettingValue) {
    if (busy.value) return
    busy.value = true
    problem.value = null
    try { await bridge.send("settings.set", { key, value }) }
    catch (err) { report(err) }
    finally {
        try { values.value = await bridge.send("settings.get", {}); publishAudioLevels(values.value); applyWidgetSettings(values.value) }
        catch (err) { report(err) }
        busy.value = false
    }
}
async function applyPreset(id: string) {
    const chosen = GRAPHICS_PRESETS.find((p) => p.id === id)
    if (!chosen || busy.value) return
    busy.value = true
    problem.value = null
    try { await bridge.send("settings.apply", { values: { ...chosen.values } }) }
    catch (err) { report(err) }
    finally {
        try { values.value = await bridge.send("settings.get", {}); publishAudioLevels(values.value); applyWidgetSettings(values.value) }
        catch (err) { report(err) }
        busy.value = false
    }
}
function open(m: SearchMatch) {
    categoryId.value = m.category.id
    pageId.value = m.page.id
    selectedKey.value = m.key
}
onMounted(async () => {
    try { values.value = await bridge.send("settings.get", {}); publishAudioLevels(values.value); applyWidgetSettings(values.value) }
    catch (err) { report(err) }
})
</script>

<template>
    <WorkbenchPanel ref="panel" v-model:group="railChoice" v-model:query="query" class="settings"
        title="Settings" :section="panelSection" :groups="RAIL" rail-label="Settings categories"
        search-label="Search settings" search-placeholder="Search settings…">
        <template #actions><Button v-if="closeLabel" size="sm" @click="emit('close')">{{ closeLabel }}</Button></template>
        <template #list>
            <template v-if="query.trim()">
                <template v-for="c in CATEGORIES" :key="c.id">
                    <ListGroup v-if="matches.some((m) => m.category.id === c.id)" :title="c.title">
                        <ListItem v-for="m in matches.filter((item) => item.category.id === c.id)" :key="m.key"
                            :on="selectedKey === m.key" @click="open(m)">
                            {{ m.label }}<template #sub>{{ m.page.title }}</template>
                        </ListItem>
                    </ListGroup>
                </template>
                <ListEmpty v-if="!matches.length">No settings match “{{ query.trim() }}”.</ListEmpty>
            </template>
            <template v-else>
                <ListGroup v-for="group in pageGroups" :key="group.title" :title="group.title">
                    <ListItem v-for="p in group.pages" :key="p.id" :on="pageId === p.id" @click="pageId = p.id">{{ p.title }}</ListItem>
                </ListGroup>
            </template>
        </template>

        <template #content>
            <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
            <p v-if="!values && !problem" class="dim">Reading settings…</p>
            <header class="page-intro">
                <h2>{{ page.title }}</h2>
                <p>{{ page.note }}</p>
            </header>
            <template v-if="values && page.preset">
                <Choice v-model="chosenPreset" :options="presetOptions" :disabled="busy" />
                <p class="dim">{{ preset ? "" : "Custom: your settings match no preset." }}</p>
            </template>
            <Keybindings v-if="page.bindings" :focus-key="focusBinding" />
            <div v-if="values && page.options.length" class="options">
                <div v-for="o in page.options" :key="o.key" class="option" :class="{ on: selectedKey === o.key, unavailable: !available(o) }"
                    @pointerdown="selectedKey = o.key" @focusin="selectedKey = o.key">
                    <Slider v-if="o.kind === 'slider'" :model-value="numberValue(o.key)" :label="o.label"
                        :min="o.min" :max="o.max" :step="o.step" :format="o.format" :disabled="busy || !available(o)"
                        @update:model-value="stage(o.key, $event)" @change="save(o.key, $event)" />
                    <Toggle v-else-if="o.kind === 'toggle'" :model-value="flagValue(o.key)" :label="o.label" :disabled="busy || !available(o)"
                        @update:model-value="save(o.key, $event)" />
                    <template v-else>
                        <h3 class="option-label">{{ o.label }}</h3>
                        <Segmented :model-value="choiceValue(o.key)" :options="o.choices"
                            @update:model-value="$event !== null && save(o.key, $event)" />
                    </template>
                    <p class="option-note">{{ !available(o) && o.requires?.note ? o.requires.note : o.note }}<span v-if="o.applies" class="applies"> {{ APPLIES[o.applies] }}</span></p>
                </div>
            </div>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
.page-intro { display: grid; gap: 7px; padding-bottom: 16px; border-bottom: 1px solid var(--stone-4); }
.page-intro h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
.page-intro p, .option-note { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.options { display: grid; }
.option { padding: 12px 10px; border: var(--b) solid transparent; border-bottom-color: var(--stone-3); }
.option.on { border-color: var(--edge); background: var(--stone-1); box-shadow: var(--sunk); }
.option.unavailable { opacity: 0.45; }
.option.unavailable :deep(.segmented) { pointer-events: none; }
.option-label { margin: 0 0 8px; font: 12px var(--font-display); text-transform: uppercase; }
.option-note { max-width: 68ch; padding-top: 6px; }
.applies { color: var(--ink-faint); }
.settings :deep(.workbench-content-inner) { padding-bottom: 48px; }
</style>
