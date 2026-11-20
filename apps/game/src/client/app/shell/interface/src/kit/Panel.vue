<script setup lang="ts">
/** A framed slab with a title bar: the container every menu screen sits in. */
defineProps<{ title?: string }>()
</script>

<template>
    <section class="panel">
        <header v-if="title || $slots.actions">
            <slot name="lead" />
            <h2 v-if="title"><span class="chev">»</span> {{ title }}</h2>
            <div class="actions"><slot name="actions" /></div>
        </header>
        <div class="body"><slot /></div>
        <footer v-if="$slots.footer"><slot name="footer" /></footer>
    </section>
</template>

<style scoped>
.panel {
    display: flex;
    flex-direction: column;
    max-height: 100%;
    border: var(--b) solid var(--edge);
    background: rgb(21 24 27 / 0.94);
    box-shadow: var(--raised), 8px 8px 0 rgb(0 0 0 / 0.35);
}

header {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 8px 14px;
    border-bottom: var(--b) solid var(--edge);
    background: var(--stone-3);
    box-shadow: var(--raised);
}

h2 {
    margin: 0;
    font: 400 16px var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.06em;
}

.chev { color: var(--accent); }
.actions { margin-left: auto; display: flex; gap: 8px; }
.body { padding: 18px; overflow: hidden auto; min-height: 0; min-width: 0; flex: 1; background: var(--stone-1); }

footer {
    display: flex;
    justify-content: flex-end;
    align-items: center;
    gap: 10px;
    padding: 10px 14px;
    border-top: var(--b) solid var(--edge);
    background: var(--stone-1);
    box-shadow: var(--raised);
}
</style>
