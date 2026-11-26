<script setup lang="ts">
/**
 * What this mod needs, and what needs it, as the website's (registry/detail/dependencies.vue):
 * each one opens its own page. The registry doesn't say what needs a mod yet, and that side says so.
 */
defineProps<{ needs: readonly (readonly [string, string])[] }>()
defineEmits<{ open: [name: string] }>()
</script>

<template>
    <div class="dependencies">
        <section>
            <h3>Needs <span>{{ needs.length }}</span></h3>
            <ul v-if="needs.length">
                <li v-for="[name, range] in needs" :key="name">
                    <button type="button" @click="$emit('open', name)">{{ name }}</button>
                    <code>{{ range }}</code>
                </li>
            </ul>
            <p v-else>Nothing: it runs on the game alone.</p>
        </section>
        <section>
            <h3>Needed by</h3>
            <p>The registry doesn't list what needs a mod yet.</p>
        </section>
    </div>
</template>

<style scoped>
.dependencies { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1rem; }
section { padding: 1.25rem 1.5rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); }
h3 { margin: 0 0 1rem; color: var(--ink); font: 400 0.68rem var(--font-display); text-transform: uppercase; }
h3 span { margin-left: 0.4rem; color: var(--ink-faint); }
ul { display: grid; gap: 0.5rem; margin: 0; padding: 0; list-style: none; }
li { display: flex; justify-content: space-between; gap: 1rem; padding: 0.6rem 0.8rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); font: 0.8rem var(--font); }
li button { padding: 0; border: 0; background: none; color: var(--accent-hi); font: inherit; text-align: left; }
li button:hover { color: var(--ink); }
code { color: var(--ink-faint); font: inherit; }
p { margin: 0; color: var(--ink-faint); font-size: 0.85rem; }
@media (max-width: 900px) { .dependencies { grid-template-columns: minmax(0, 1fr); } }
</style>
