<script setup lang="ts">
/**
 * One row of a workspace list. `lead` is a fixed column (a formula, an
 * initial) so names line up; `sub` a second line; `trail` a status.
 * `faint` greys a row that's there but not yet yours.
 */
defineProps<{ on?: boolean; faint?: boolean }>()
</script>

<template>
    <button type="button" class="list-item" :class="{ on, faint, lead: !!$slots.lead }" :aria-current="on ? 'true' : undefined">
        <span v-if="$slots.lead" class="lead"><slot name="lead" /></span>
        <span class="text">
            <span class="name"><slot /></span>
            <small v-if="$slots.sub"><slot name="sub" /></small>
        </span>
        <span v-if="$slots.trail" class="trail"><slot name="trail" /></span>
    </button>
</template>

<style scoped>
.list-item { display: grid; grid-template-columns: minmax(0, 1fr) auto; align-items: center; gap: 0 8px; width: 100%; margin-bottom: 3px; padding: 6px 8px;
    border: var(--b) solid transparent; background: none; color: var(--ink-dim); font-size: 12px; text-align: left; }
.list-item.lead { grid-template-columns: 52px minmax(0, 1fr) auto; }
.list-item:hover { color: var(--ink); background: var(--stone-1); }
.list-item.on { border-color: var(--edge); background: var(--accent-lo); color: var(--ink); box-shadow: var(--sunk); }
.list-item.faint { opacity: 0.45; }
.lead { font: 12px var(--font-display); color: var(--ink); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.text { display: flex; flex-direction: column; min-width: 0; }
.name, small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
small { color: var(--ink-faint); font-size: 10px; }
.trail { color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
</style>
