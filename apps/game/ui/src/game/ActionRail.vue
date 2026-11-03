<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { Ask, StateSections } from "../bridge"
import { useBridge } from "../bridge/use.ts"
import Button from "../kit/Button.vue"
import Field from "../kit/Field.vue"
import IconButton from "../kit/IconButton.vue"
import PixelIcon from "./PixelIcon.vue"
import { ACTION_ICONS, INK } from "./toolIcons.ts"

/**
 * The object's tools, low on the right while building (sprints 53–55): mirror, level, reset, round (sprint 75), hollow, undo; pick up
 * and a new object; the parts library (sprint 74: every world's; a part taken from it is carried till you click,
 * as Ctrl+V's paste is). A part's own menu is right-clicked on it, in the world. Clickable where the mouse is free (N,
 * or paused). Ctrl+S asks for a name here.
 */
// free: the mouse is (an object held, or paused)
const props = defineProps<{ tools: Extract<StateSections["tools"], { out: true }>; library: string[]; free: boolean }>()
const bridge = useBridge()
type Pop = "save" | "library"
const open = ref<Pop | null>(null)
const name = ref("")
// as src/workshop/library.h: Library::named
const LONGEST_NAME = 32
const named = computed(() => /^[A-Za-z0-9_-]([A-Za-z0-9 _-]*[A-Za-z0-9_-])?$/.test(name.value) && name.value.length <= LONGEST_NAME)

const ask = (a: Ask) => { void bridge.send("tools.ask", a) }
const toggle = (which: Pop) => { open.value = open.value === which ? null : which }
const save = () => {
    if (!named.value) return
    ask({ action: "save", name: name.value })
    open.value = null
}
const take = (part: string) => {
    ask({ action: "take", name: part })
    open.value = null
}
const place = (part: string) => {
    ask({ action: "place", name: part })
    open.value = null
}
// Ctrl+S: the game's freed the cursor and asks the part's name
watch(() => props.tools.naming, (naming) => { if (naming) open.value = "save" }, { immediate: true })
</script>

<template>
    <nav class="action-rail rail" :class="{ free: free }" aria-label="Object">
        <IconButton :label="ACTION_ICONS.mirror.label" :on="tools.mirrored" side="left" @click="ask({ action: 'mirror' })">
            <PixelIcon :rows="ACTION_ICONS.mirror.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton :label="ACTION_ICONS.level.label" :disabled="!tools.focused" side="left" @click="ask({ action: 'level' })">
            <PixelIcon :rows="ACTION_ICONS.level.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton :label="ACTION_ICONS.reset.label" :disabled="!tools.focused" side="left" @click="ask({ action: 'reset' })">
            <PixelIcon :rows="ACTION_ICONS.reset.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton :label="ACTION_ICONS.round.label" :disabled="!tools.focused" side="left" @click="ask({ action: 'round' })">
            <PixelIcon :rows="ACTION_ICONS.round.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton :label="ACTION_ICONS.hollow.label" :disabled="!tools.focused" side="left" @click="ask({ action: 'hollow' })">
            <PixelIcon :rows="ACTION_ICONS.hollow.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton :label="ACTION_ICONS.undo.label" side="left" @click="ask({ action: 'undo' })">
            <PixelIcon :rows="ACTION_ICONS.undo.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <div class="gap" />
        <IconButton :label="ACTION_ICONS.focus.label" :on="tools.focused" side="left" @click="ask({ action: 'focus' })">
            <PixelIcon :rows="ACTION_ICONS.focus.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <IconButton :label="ACTION_ICONS.start.label" side="left" @click="ask({ action: 'start' })">
            <PixelIcon :rows="ACTION_ICONS.start.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
        <div class="gap" />
        <IconButton :label="ACTION_ICONS.library.label" :on="open === 'library'" side="left" @click="toggle('library')">
            <PixelIcon :rows="ACTION_ICONS.library.rows" :palette="INK" style="--u: 2px" />
            <span v-if="library.length" class="count">{{ library.length }}</span>
        </IconButton>
        <IconButton :label="ACTION_ICONS.save.label" :on="open === 'save'" side="left" @click="toggle('save')">
            <PixelIcon :rows="ACTION_ICONS.save.rows" :palette="INK" style="--u: 2px" />
        </IconButton>
    </nav>

    <!-- what's open beside the rail, above it: never over the middle of the screen -->
    <div class="beside" :class="{ free: free }">
        <div v-if="open === 'library'" class="pop">
            <h3>Parts</h3>
            <p v-if="!library.length" class="hint">None yet: point at a part and press Ctrl+S.</p>
            <ul v-else>
                <li v-for="b in library" :key="b" class="row">
                    <button class="name" title="Carry one (click to put it on)" @click="take(b)">{{ b }}</button>
                    <button class="small" title="Put one on the ground where you point" @click="place(b)">⇣</button>
                    <button class="small" title="Forget it" @click="ask({ action: 'forget', name: b })">×</button>
                </li>
            </ul>
            <p class="hint">Ctrl+C copies the part you point at (and what hangs from it) · Ctrl+V pastes · Ctrl+S saves it here, in every world.</p>
        </div>
        <form v-if="open === 'save'" class="pop" @submit.prevent="save">
            <h3>Save part</h3>
            <p class="hint">The part you point at, and what hangs from it; the first part: all of it.</p>
            <Field v-model="name" label="Name" placeholder="spoked wheel" :maxlength="LONGEST_NAME" />
            <Button variant="primary" size="sm" :disabled="!named">Save</Button>
        </form>
    </div>
</template>

<style scoped>
/* low on the right, a third of the screen at most, wrapping into columns to the left */
.action-rail {
    position: absolute; right: 20px; bottom: 110px;
    display: flex; flex-direction: column; flex-wrap: wrap-reverse; align-content: flex-start;
    gap: 10px; max-height: 33vh;
}
.action-rail.free, .beside.free { pointer-events: auto; }
.gap { height: 6px; width: 48px; }
.count {
    position: absolute; top: -7px; right: -7px; min-width: 18px; height: 18px; padding: 0 3px;
    background: var(--accent); border: 1px solid var(--edge); font: 10px/16px var(--font-display);
}
.beside {
    position: absolute; right: 20px; bottom: calc(110px + 33vh + 16px);
    display: flex; flex-direction: column; align-items: flex-end; gap: 10px; max-width: 300px;
}
.pop {
    display: flex; flex-direction: column; gap: 10px; width: 260px; padding: 12px;
    border: var(--b) solid var(--edge); background: rgb(21 24 27 / 0.94); box-shadow: var(--raised), 8px 8px 0 rgb(0 0 0 / 0.35);
}
h3 { margin: 0; font: 12px var(--font-display); text-transform: uppercase; color: var(--ink); }
.row { display: flex; gap: 6px; }
ul { display: flex; flex-direction: column; gap: 4px; max-height: 30vh; margin: 0; padding: 0; overflow-y: auto; list-style: none; }
li .name { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
li .small { width: 34px; padding: 8px 0; text-align: center; }
li button {
    width: 100%; padding: 8px 10px; text-align: left; color: var(--ink); background: var(--stone-2);
    border: var(--b) solid var(--edge); font: 13px var(--font-body, inherit);
}
li button:hover { background: var(--stone-3); outline: var(--b) solid var(--accent); }
.hint { margin: 0; font-size: 12px; color: var(--ink-faint); }
</style>
