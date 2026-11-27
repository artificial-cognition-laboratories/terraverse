<script setup lang="ts">
/**
 * One setting of a form: what it is and why on the left, its control on the right at a set width,
 * so a column of them lines up. Rows are divided by a hairline, as the site's dashboard forms are.
 */
defineProps<{ title: string; description?: string }>()
</script>

<template>
    <div class="form-row">
        <div class="about">
            <h3>{{ title }}</h3>
            <p v-if="description">{{ description }}</p>
        </div>
        <div class="control"><slot /></div>
    </div>
</template>

<style scoped>
.form-row { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 22rem); gap: 5rem; align-items: start; padding: 1.25rem 0; border-top: 1px solid var(--glass-edge); }
.form-row:first-child { border-top: 0; padding-top: 0; }
h3 { margin: 0 0 0.35rem; color: var(--ink); font: 400 0.66rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
p { margin: 0; color: var(--ink-faint); font-size: 0.78rem; line-height: 1.5; }
.control { display: grid; gap: 0.5rem; min-width: 0; }
@media (max-width: 760px) { .form-row { grid-template-columns: minmax(0, 1fr); gap: 0.75rem; } }
</style>
