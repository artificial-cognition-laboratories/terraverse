<script setup lang="ts">
/**
 * Saves a report and copies where it went: the path's on the clipboard, ready
 * to paste to an agent. A tick for a moment says it worked; a cross, with the
 * reason on hover, says it didn't (and copying a path that was saved but
 * couldn't be copied says so the same way).
 */
import { ref } from "vue"
import { useBridge } from "../../bridge/use.ts"

const props = defineProps<{ save: () => Promise<string>; title: string }>()

const SHOWN_MS = 1200
const bridge = useBridge()
const saving = ref(false)
const state = ref<"idle" | "done" | "failed">("idle")
const problem = ref("")
let timer = 0

async function run() {
    saving.value = true
    clearTimeout(timer)
    try {
        const path = await props.save()
        await bridge.send("clipboard.copy", { text: path })
        state.value = "done"
        timer = window.setTimeout(() => (state.value = "idle"), SHOWN_MS)
    } catch (error) {
        problem.value = error instanceof Error ? error.message : String(error)
        state.value = "failed"
        timer = window.setTimeout(() => (state.value = "idle"), SHOWN_MS * 4)
    } finally {
        saving.value = false
    }
}
</script>

<template>
    <button class="icon" :class="state" :title="state === 'failed' ? problem : `${title}, and copy its path`" :aria-label="title" :disabled="saving" @click="run">
        <svg v-if="state === 'done'" viewBox="0 0 16 16" aria-hidden="true"><path d="M3 8.5l3.2 3L13 4.5" /></svg>
        <svg v-else-if="state === 'failed'" viewBox="0 0 16 16" aria-hidden="true"><path d="M4 4l8 8M12 4l-8 8" /></svg>
        <svg v-else viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2v8M4.5 6.5L8 10l3.5-3.5M2.5 11v2.5h11V11" /></svg>
    </button>
</template>

<style scoped>
.icon { display: flex; align-items: center; justify-content: center; width: 26px; height: 26px; border: 0; border-radius: 4px; background: none; color: var(--dt-dim); cursor: default; }
.icon:hover:not(:disabled) { color: var(--dt-ink); background: var(--dt-hover); }
.icon:disabled { opacity: 0.5; }
.icon.done { color: #5fbf72; }
.icon.failed { color: var(--dt-error); }
svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
</style>
