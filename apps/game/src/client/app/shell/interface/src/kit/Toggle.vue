<script setup lang="ts">
/** On/off, as the website's switch: the label, then a square track whose knob slides over and lights. */
defineProps<{ label: string }>()
const model = defineModel<boolean>({ required: true })
</script>

<template>
    <button class="toggle" :class="{ on: model }" type="button" role="switch" :aria-checked="model" @click="model = !model">
        <span class="label">{{ label }}</span>
        <span class="track" aria-hidden="true"><span class="knob" /></span>
    </button>
</template>

<style scoped>
.toggle { display: flex; align-items: center; gap: 0.75rem; width: 100%; padding: 0.25rem; border: 0; background: none; text-align: left; }
.label { flex: 1; min-width: 0; color: var(--ink-dim); font: 0.88rem var(--font-sans); }
.track { position: relative; flex: none; width: 2.2rem; height: 1.1rem; border: 1px solid var(--night-4); border-radius: var(--radius); transition: background 0.12s, border-color 0.12s; }
.knob { position: absolute; top: 2px; left: 2px; width: calc(1.1rem - 6px); height: calc(1.1rem - 6px); background: var(--ink-faint); transition: transform 0.12s, background 0.12s; }
.toggle:hover .label, .on .label { color: var(--ink); }
.on .track { background: rgb(10 75 145 / 0.5); border-color: var(--accent-hi); }
.on .knob { background: #fff; transform: translateX(1.1rem); }
</style>
