<script setup lang="ts">
/** On/off as a two-state block switch. */
defineProps<{ label: string }>()
const model = defineModel<boolean>({ required: true })
</script>

<template>
    <button class="toggle" :class="{ on: model }" role="switch" :aria-checked="model" @click="model = !model">
        <span class="label">{{ label }}</span>
        <span class="track"><span class="knob" /></span>
    </button>
</template>

<style scoped>
.toggle {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 16px;
    width: 100%;
    text-align: left;
    padding: 0;
    border: 0;
    background: none;
    font-size: 13px;
    color: var(--ink-dim);
}

.label { flex: 1; min-width: 0; }

.track {
    position: relative;
    flex: none;
    width: 44px;
    height: 22px;
    border: var(--b) solid var(--edge);
    background: var(--stone-0);
    box-shadow: var(--sunk);
}

.knob {
    position: absolute;
    top: 1px;
    left: 1px;
    width: 16px;
    height: 16px;
    background: var(--stone-4);
    box-shadow: var(--raised);
    transition: left 90ms steps(3);
}

.on .track { background: var(--accent-lo); }
.on .knob { left: 23px; background: var(--accent-hi); }
.toggle:hover .label { color: var(--ink); }
</style>
