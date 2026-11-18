<script setup lang="ts">
/**
 * Steps in time, one row each: a label, when it ended (from the first step's
 * start), and a bar over the whole span from its start to its end, so the long
 * ones show as long bars. Strong rows are milestones; the rest sit between them.
 */
import { computed } from "vue"

export type Step = { key: string; label: string; start: number; end: number; color: string; strong?: boolean }

const props = defineProps<{ steps: readonly Step[]; summary?: string }>()

const laid = computed(() => {
    if (!props.steps.length) return []
    const t0 = Math.min(...props.steps.map((s) => s.start))
    const total = Math.max(Math.max(...props.steps.map((s) => s.end)) - t0, 1)
    return props.steps.map((s) => ({
        ...s, took: s.end - s.start, at: s.end - t0,
        from: ((s.start - t0) / total) * 100, width: Math.max(0.4, ((s.end - s.start) / total) * 100),
    }))
})
const time = (ms: number) => (ms >= 1000 ? `${(ms / 1000).toFixed(2)} s` : ms >= 10 ? `${Math.round(ms)} ms` : `${ms.toFixed(1)} ms`)
</script>

<template>
    <div class="waterfall">
        <p v-if="summary" class="sum">{{ summary }}</p>
        <ol>
            <li v-for="s in laid" :key="s.key" :class="{ strong: s.strong }" :title="`${s.label}: ${time(s.took)}, done at ${time(s.at)}`">
                <span class="label"><i :style="{ background: s.color }" />{{ s.label }}</span>
                <span class="took">{{ time(s.took) }}</span>
                <span class="track"><span class="bar" :style="{ left: `${s.from}%`, width: `${s.width}%`, background: s.color }" /></span>
            </li>
        </ol>
    </div>
</template>

<style scoped>
.waterfall { height: 100%; overflow-y: auto; font-size: 12px; }
.sum { margin: 0; padding: 8px 10px; border-bottom: 1px solid var(--dt-line); color: var(--dt-dim); }
ol { margin: 0; padding: 4px 0; list-style: none; }
li { display: grid; grid-template-columns: minmax(0, 200px) 64px minmax(0, 1fr); align-items: center; gap: 8px; height: 20px; padding: 0 10px; }
li:hover { background: var(--dt-hover); }
.label { display: flex; align-items: center; gap: 6px; min-width: 0; overflow: hidden; color: var(--dt-dim); white-space: nowrap; text-overflow: ellipsis; }
.label i { flex: none; width: 8px; height: 8px; border-radius: 2px; }
.strong .label { color: var(--dt-ink); font-weight: 600; }
.took { color: var(--dt-faint); font: 11px ui-monospace, monospace; text-align: right; }
.track { position: relative; height: 8px; border-radius: 2px; background: rgb(255 255 255 / 0.04); }
.bar { position: absolute; top: 0; bottom: 0; border-radius: 2px; opacity: 0.85; }
.strong .bar { opacity: 1; }
</style>
