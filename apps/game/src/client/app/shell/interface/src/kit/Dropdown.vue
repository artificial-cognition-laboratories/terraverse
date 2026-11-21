<script setup lang="ts" generic="T extends string">
import { computed, onBeforeUnmount, ref } from "vue"

/**
 * Pick one of a list from a closed box: the choice shown, the list dropped below it.
 * Drawn in the page, not a native <select>, whose popup the offscreen browser can't show.
 */
const props = defineProps<{ options: readonly { value: T; label: string }[]; label: string }>()
const model = defineModel<T>({ required: true })
const open = ref(false)
const root = ref<HTMLElement | null>(null)
const shown = computed(() => props.options.find((o) => o.value === model.value)?.label ?? "")
function pick(value: T) {
    model.value = value
    open.value = false
}
function outside(event: PointerEvent) {
    if (root.value && !root.value.contains(event.target as Node)) open.value = false
}
document.addEventListener("pointerdown", outside)
onBeforeUnmount(() => document.removeEventListener("pointerdown", outside))
</script>

<template>
    <div ref="root" class="dropdown" :class="{ open }" @keydown.escape="open = false">
        <button type="button" class="box" :aria-label="label" aria-haspopup="listbox" :aria-expanded="open" @click="open = !open">
            <span>{{ shown }}</span><span class="chev">▾</span>
        </button>
        <ul v-if="open" role="listbox">
            <li v-for="o in options" :key="o.value">
                <button type="button" role="option" :aria-selected="o.value === model" :class="{ on: o.value === model }" @click="pick(o.value)">
                    {{ o.label }}<span v-if="o.value === model">✓</span>
                </button>
            </li>
        </ul>
    </div>
</template>

<style scoped>
.dropdown { position: relative; min-width: 140px; }
.box { display: flex; justify-content: space-between; align-items: center; gap: 10px; width: 100%; height: 26px; padding: 0 8px; border: 1px solid var(--stone-3); background: none; color: var(--ink); font-size: 11px; white-space: nowrap; }
.box:hover, .open .box { border-color: var(--stone-4); }
.chev { color: var(--ink-faint); font-size: 10px; }
.open .chev { transform: rotate(180deg); }
ul { position: absolute; z-index: 10; top: calc(100% + 2px); left: 0; right: 0; margin: 0; padding: 3px 0; list-style: none; border: 1px solid var(--stone-4); background: var(--stone-1); box-shadow: 4px 4px 0 rgb(0 0 0 / 0.35); }
ul button { display: flex; justify-content: space-between; width: 100%; padding: 6px 8px; border: 0; background: none; color: var(--ink-dim); font-size: 11px; text-align: left; white-space: nowrap; }
ul button:hover { background: var(--stone-2); color: var(--ink); }
ul button.on { color: var(--ink); }
ul button span { color: var(--accent-hi); }
</style>
