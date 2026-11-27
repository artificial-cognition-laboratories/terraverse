<script setup lang="ts">
/**
 * Where the time goes, three ways. Live: charts of every frame as it streams,
 * smooth because they're drawn from the frames alone. Flame: a capture (every
 * thread's zones over a few seconds, fetched once) to look through without
 * waiting on the engine. Stats: every value the old profiler reported, as a
 * table, any of them pinned to chart in Live. The recorder is always on, so capturing is just
 * saying when: the last few seconds, or a moment clicked on a Live chart.
 */
import { ref } from "vue"
import ReportButton from "../kit/ReportButton.vue"
import SubTabs from "../kit/SubTabs.vue"
import FlamePane from "./FlamePane.vue"
import LiveCharts from "./LiveCharts.vue"
import StatsTab from "./StatsTab.vue"
import type { PerfStoreHandle } from "./store.ts"

const props = defineProps<{ perf: PerfStoreHandle; exportTrace: () => Promise<string> }>()
const perf = props.perf

/** What a capture covers: the engine keeps about ten seconds of a busy thread. */
const CAPTURE_MS = 3000
const VIEWS = [{ id: "live", label: "Live" }, { id: "flame", label: "Flame" }, { id: "stats", label: "Stats" }] as const
const view = ref<(typeof VIEWS)[number]["id"]>("live")
const statsText = ref("")
/** How much time Live shows; the wheel over it sets this. */
const liveSpan = ref(15_000)
const spanLabel = (ms: number) => (ms >= 1000 ? `${(ms / 1000).toFixed(ms >= 10_000 ? 0 : 1)} s` : `${Math.round(ms)} ms`)

async function captureAt(at: number | null) {
    const list = perf.frames.value
    const last = list[list.length - 1]
    if (!last) return
    // a moment clicked: mostly the time before it, which is what led up to it
    const to = at === null ? last.end : Math.min(last.end, at + CAPTURE_MS / 3)
    view.value = "flame"
    await perf.capture(Math.max(0, to - CAPTURE_MS), to)
}

</script>

<template>
    <div class="perf">
        <div class="bar">
            <SubTabs v-model="view" :tabs="VIEWS" label="Performance views" />
            <label v-if="view === 'stats'" class="search" title="Click a row to pin it in Live">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M2 3h12l-4.5 5.5V13l-3-1.5v-3z" /></svg>
                <input v-model="statsText" type="search" placeholder="Filter values" aria-label="Filter values" spellcheck="false" />
            </label>
            <span class="gap" />
            <span v-if="view === 'live'" class="span" title="The time Live shows: Shift-wheel over the charts sets it">{{ spanLabel(liveSpan) }}</span>
            <button class="action" :disabled="perf.capturing.value || !perf.frames.value.length" title="Every thread's zones over the last 3 seconds"
                @click="captureAt(null)">
                <svg viewBox="0 0 16 16" aria-hidden="true"><circle cx="8" cy="8" r="5.5" /><circle cx="8" cy="8" r="2.2" class="dot" /></svg>
                {{ perf.capturing.value ? "Capturing…" : "Capture" }}
            </button>
            <ReportButton :save="exportTrace" title="Save everything recorded as a report (summary.md, frames and spans as lines, a Perfetto trace)" />
        </div>
        <p v-if="perf.problem.value" class="note bad" @click="perf.problem.value = null">{{ perf.problem.value }}</p>

        <LiveCharts v-if="view === 'live'" class="view" :frames="perf.frames.value" :units="perf.units.value" :pinned="perf.pinned.value"
            @capture="captureAt" @span="(ms: number) => (liveSpan = ms)" />
        <StatsTab v-else-if="view === 'stats'" class="view" :perf="perf" :text="statsText" />
        <template v-else>
            <FlamePane v-if="perf.current.value" class="view" :capture="perf.current.value" />
            <div v-else class="empty">
                <p>No capture yet.</p>
                <p class="faint">Capture the last three seconds, or click a moment on a Live chart.</p>
            </div>
        </template>
    </div>
</template>

<style scoped>
.perf { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.bar { display: flex; align-items: center; gap: 6px; flex: none; height: 30px; padding: 0 6px 0 0; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
.search { display: flex; align-items: center; gap: 4px; width: 170px; min-width: 0; height: 22px; padding: 0 6px; border-radius: 4px; color: var(--dt-faint); }
.search:focus-within { background: var(--dt-hover); outline: 1px solid var(--dt-accent); }
input { flex: 1; min-width: 0; border: 0; outline: 0; background: none; color: var(--dt-ink); font: inherit; font-size: 12px; }
input::placeholder { color: var(--dt-faint); }
button { display: flex; align-items: center; border: 0; background: none; color: var(--dt-dim); font: inherit; font-size: 12px; cursor: default; }
.gap { flex: 1; }
.span { color: var(--dt-faint); font: 11px ui-monospace, monospace; white-space: nowrap; }
.action { gap: 6px; height: 24px; padding: 0 8px; border-radius: 4px; color: var(--dt-ink); }
.action:hover:not(:disabled) { background: var(--dt-hover); }
.action:disabled { opacity: 0.5; }
.icon { justify-content: center; width: 26px; height: 26px; border-radius: 4px; }
.icon:hover { color: var(--dt-ink); background: var(--dt-hover); }
svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
svg .dot { fill: #ff6b6b; stroke: none; }
.view { flex: 1; min-height: 0; }
.empty { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 40px 16px; color: var(--dt-dim); text-align: center; }
.empty p { margin: 0; }
.faint { color: var(--dt-faint); }
.note { margin: 0; padding: 6px 10px; border-bottom: 1px solid var(--dt-line); color: var(--dt-dim); font-size: 12px; cursor: default; }
.note code { color: var(--dt-ink); user-select: text; }
.note.bad { color: var(--dt-error); background: var(--dt-error-bg); }
</style>
