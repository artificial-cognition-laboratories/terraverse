<script setup lang="ts">
import { Notice, Panel } from "@game/kit/index.ts"

/** The launcher couldn't start: say why, on screen, rather than leave a blank window. */
defineProps<{ error: string }>()
</script>

<template>
    <div class="fault">
        <Panel title="The launcher couldn't start">
            <div class="body">
                <Notice tone="danger">{{ error }}</Notice>
                <p>Run with the mock backend (<code>bun run dev:mock</code>) until this command is wired.</p>
            </div>
        </Panel>
    </div>
</template>

<style scoped>
.fault { display: grid; place-items: center; height: 100%; padding: 16px; background: var(--stone-0); }
.body { display: flex; flex-direction: column; gap: 12px; width: 520px; max-width: 100%; }
p { margin: 0; color: var(--ink-dim); font-size: 13px; }
code { color: var(--ink); }
</style>
