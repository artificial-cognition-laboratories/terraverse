<script setup lang="ts">
import { computed } from "vue"

/** A notched track with a block handle. Emits `change` on release, `update` while dragging. */
const props = defineProps<{ label: string; min: number; max: number; step: number; format?: (v: number) => string }>()
const model = defineModel<number>({ required: true })
const emit = defineEmits<{ change: [value: number] }>()

const fill = computed(() => ((model.value - props.min) / (props.max - props.min)) * 100)
const shown = computed(() => (props.format ? props.format(model.value) : String(model.value)))
</script>

<template>
    <label class="slider">
        <span class="row"><span>{{ label }}</span><span class="value">{{ shown }}</span></span>
        <input v-model.number="model" type="range" :min="min" :max="max" :step="step"
            :style="{ '--fill': `${fill}%` }" @change="emit('change', model)">
    </label>
</template>

<style scoped>
.slider { display: flex; flex-direction: column; gap: 0.4rem; }
.row { display: flex; justify-content: space-between; color: var(--ink-dim); font-size: 0.8rem; }
.value { color: var(--ink); font: 0.72rem var(--font-display); }
/* a hairline track, filled with the ocean to the value; the thumb a small block of glass */
input { appearance: none; height: 0.5rem; margin: 0; border: 1px solid var(--glass-edge); border-radius: var(--radius); outline: none;
    background: linear-gradient(90deg, var(--accent-lo) var(--fill), rgb(4 7 11 / 0.6) var(--fill)); }
input::-webkit-slider-thumb { appearance: none; width: 0.75rem; height: 1.25rem; border: 1px solid var(--edge-lit-hi); border-radius: var(--radius); background: var(--night-2); cursor: pointer; }
input:hover::-webkit-slider-thumb { border-color: var(--accent-hi); }
</style>
