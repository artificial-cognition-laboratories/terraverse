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
.dropdown { position: relative; min-width: 9rem; }
.box { display: flex; justify-content: space-between; align-items: center; gap: 0.6rem; width: 100%; height: 2.1rem; padding: 0 0.6rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); color: var(--ink); font-size: 0.75rem; white-space: nowrap; }
.box:hover, .open .box { border-color: var(--edge-lit); }
.chev { color: var(--ink-faint); font-size: 0.7rem; }
.open .chev { transform: rotate(180deg); }
ul { position: absolute; z-index: 10; top: calc(100% + 2px); left: 0; right: 0; margin: 0; padding: 0.2rem 0; list-style: none; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass-strong); backdrop-filter: blur(8px); box-shadow: 0 12px 30px rgb(0 0 0 / 0.5); }
ul button { display: flex; justify-content: space-between; width: 100%; padding: 0.45rem 0.6rem; border: 0; background: none; color: var(--ink-dim); font-size: 0.75rem; text-align: left; white-space: nowrap; }
ul button:hover { background: rgb(231 237 244 / 0.05); color: var(--ink); }
ul button.on { color: var(--ink); }
ul button span { color: var(--accent-hi); }
</style>
