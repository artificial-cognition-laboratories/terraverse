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
/* the website's side nav (shared/side/nav.vue): the chosen one lit along its left edge */
.list-item { display: grid; grid-template-columns: minmax(0, 1fr) auto; align-items: center; gap: 0 0.5rem; width: 100%; margin-bottom: 0.15rem; padding: 0.5rem 0.65rem;
    border: 0; background: none; box-shadow: inset 2px 0 0 transparent; color: var(--ink-dim); font-size: 0.78rem; text-align: left; }
.list-item.lead { grid-template-columns: 3.2rem minmax(0, 1fr) auto; }
.list-item:hover { color: var(--ink); }
.list-item.on { color: var(--ink); background: rgb(99 163 233 / 0.07); box-shadow: inset 2px 0 0 var(--accent-hi); }
.list-item.faint { opacity: 0.45; }
.lead { overflow: hidden; color: var(--ink); font: 0.72rem var(--font-display); text-overflow: ellipsis; white-space: nowrap; }
.text { display: flex; flex-direction: column; min-width: 0; }
.name, small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
small { color: var(--ink-faint); font-size: 0.65rem; }
.trail { color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; }
</style>
