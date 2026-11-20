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
.slider { display: flex; flex-direction: column; gap: 6px; }
.row { display: flex; justify-content: space-between; font-size: 13px; color: var(--ink-dim); }
.value { color: var(--ink); font-family: var(--font-display); font-size: 12px; }

input {
    appearance: none;
    height: 12px;
    margin: 0;
    border: var(--b) solid var(--edge);
    background:
        linear-gradient(90deg, var(--accent) var(--fill), var(--stone-0) var(--fill));
    box-shadow: var(--sunk);
    outline: none;
}

input::-webkit-slider-thumb {
    appearance: none;
    width: 14px;
    height: 22px;
    border: var(--b) solid var(--edge);
    background: var(--stone-4);
    box-shadow: var(--raised);
    cursor: pointer;
}

input:hover::-webkit-slider-thumb { background: #4b545c; }
</style>
