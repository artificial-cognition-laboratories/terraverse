<script setup lang="ts">
/** A pane of glass with a title line: the container every menu screen sits in. */
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
    border: 1px solid var(--glass-edge);
    background: var(--glass-strong);
    backdrop-filter: blur(8px); /* blurs the page behind it; the game's frame is under the page, so not that */
    box-shadow: var(--sheen), 0 24px 60px rgb(0 0 0 / 0.45);
}

header {
    display: flex;
    align-items: center;
    gap: 16px;
    min-height: 48px;
    padding: 8px 16px;
    border-bottom: 1px solid var(--glass-edge);
}

h2 {
    margin: 0;
    font: 400 13px var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.1em;
}

.chev { color: var(--accent-hi); }
.actions { margin-left: auto; display: flex; gap: 8px; }
.body { padding: 18px; overflow: hidden auto; min-height: 0; min-width: 0; flex: 1; }

footer {
    display: flex;
    justify-content: flex-end;
    align-items: center;
    gap: 10px;
    padding: 10px 16px;
    border-top: 1px solid var(--glass-edge);
    background: rgb(4 7 11 / 0.4);
}
</style>
