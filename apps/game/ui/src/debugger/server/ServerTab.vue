<script setup lang="ts">
/**
 * The server this game is connected to. Local (this process runs it), all of
 * it: Overview (its step, the process's CPU and memory, what it simulates, and
 * what it is, local or not among it), Players, Mods, and Startup (how it
 * started, as a waterfall). Remote, only what the connection shows, and a
 * notice saying so.
 */
import { computed, ref } from "vue"
import type { PerfFrame } from "../../bridge/index.ts"
import SubTabs from "../kit/SubTabs.vue"
import Waterfall, { type Step } from "../kit/Waterfall.vue"
import ModsView from "./ModsView.vue"
import OverviewView from "./OverviewView.vue"
import PlayersView from "./PlayersView.vue"
import type { ServerStoreHandle } from "./store.ts"

const props = defineProps<{ frames: readonly PerfFrame[]; server: ServerStoreHandle }>()
const summary = computed(() => props.server.summary.value)
const local = computed(() => (summary.value?.local ? summary.value : null))

const VIEWS = computed(() => [
    { id: "overview", label: "Overview" },
    { id: "players", label: local.value ? `Players ${local.value.players.length}` : "Players" },
    { id: "mods", label: local.value ? `Mods ${local.value.mods.length}` : "Mods" },
    { id: "startup", label: "Startup" },
] as const)
const view = ref<"overview" | "players" | "mods" | "startup">("overview")
const text = ref("")

// The last start, from its marks: "Server init" holds the rest.
const startup = computed(() => {
    const marks = props.server.marks.value.filter((m) => m.lane === "startup")
    const init = [...marks].reverse().find((m) => m.what === "Server init")
    if (!init) return null
    const run = marks.filter((m) => m.start >= init.start && m.end <= init.end).sort((a, b) => a.start - b.start || b.end - a.end)
    const steps = run.map((m): Step => {
        const mod = m.what.startsWith("mod ")
        return { key: `${m.start}:${m.what}`, label: mod ? m.what.slice(4) : m.what, start: m.start, end: m.end,
            color: mod ? "#5fbf72" : "#e6e6ee", strong: !mod }
    })
    const took = init.end - init.start
    const loads = new Map(run.filter((m) => m.what.startsWith("mod ")).map((m) => [m.what.slice(4), m.end - m.start]))
    return { steps: steps, loads: loads, summary: `Started in ${took >= 1000 ? `${(took / 1000).toFixed(2)} s` : `${Math.round(took)} ms`}` }
})
</script>

<template>
    <div class="server">
        <div class="views">
            <SubTabs v-model="view" :tabs="VIEWS" label="Server views" />
            <span class="gap" />
            <label v-if="view === 'players' || view === 'mods'" class="search">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M2 3h12l-4.5 5.5V13l-3-1.5v-3z" /></svg>
                <input v-model="text" type="search" placeholder="Filter" aria-label="Filter" spellcheck="false" />
            </label>
        </div>
        <p v-if="summary && !summary.local" class="notice">
            A remote server: the debugger sees only what the connection shows (the Network tab). Players, mods and startup are for a server this game runs.
        </p>
        <div class="content">
            <OverviewView v-if="view === 'overview'" :frames="frames" :server="server" />
            <template v-else-if="!local">
                <p class="empty">{{ summary ? "Only for a server this game runs." : "Enter a world to see its server." }}</p>
            </template>
            <PlayersView v-else-if="view === 'players'" :players="local.players" :text="text" />
            <ModsView v-else-if="view === 'mods'" :mods="local.mods" :text="text" />
            <template v-else>
                <Waterfall v-if="startup" :steps="startup.steps" :summary="startup.summary" />
                <p v-else class="empty">No start recorded: the debugger keeps marks from when this game started a server.</p>
            </template>
        </div>
    </div>
</template>

<style scoped>
.server { display: flex; flex-direction: column; height: 100%; min-height: 0; }
.gap { flex: 1; }
.views { display: flex; align-items: center; flex: none; height: 30px; padding-right: 6px; border-bottom: 1px solid var(--dt-line); background: var(--dt-rail); }
.search { display: flex; align-items: center; gap: 4px; width: 150px; height: 22px; padding: 0 6px; border-radius: 4px; color: var(--dt-faint); }
.search:focus-within { background: var(--dt-hover); outline: 1px solid var(--dt-accent); }
svg { width: 16px; height: 16px; flex: none; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
input { flex: 1; min-width: 0; border: 0; outline: 0; background: none; color: var(--dt-ink); font: inherit; font-size: 12px; }
input::placeholder { color: var(--dt-faint); }
.notice { margin: 0; padding: 6px 10px; border-bottom: 1px solid var(--dt-line); color: var(--dt-dim); background: rgb(229 163 59 / 0.08); font-size: 12px; }
.content { flex: 1; min-height: 0; }
.empty { margin: 0; padding: 32px 16px; color: var(--dt-dim); font-size: 12px; text-align: center; }
</style>
