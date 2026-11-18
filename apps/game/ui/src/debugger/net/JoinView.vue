<script setup lang="ts">
/**
 * The last join, as a waterfall: from "Connecting" to "In the world", each
 * step a row (the milestones bold, the first arrival of each message type
 * between them), its bar from the step before to it, so the long waits show
 * as long bars. The engine keeps these events, so the join is here however
 * long ago it was.
 */
import { computed } from "vue"
import type { NetEvent } from "../../bridge/index.ts"
import { categoryOf, short } from "./categories.ts"

const props = defineProps<{ events: readonly NetEvent[]; nameOf: (dir: 0 | 1, command: number) => string }>()

const join = computed(() => {
    const list = props.events
    let start = -1
    for (let i = list.length - 1; i >= 0; i--) {
        if (list[i]!.what === "Connecting") {
            start = i
            break
        }
    }
    if (start < 0) return null
    const steps = list.slice(start).sort((a, b) => a.time - b.time)
    const t0 = steps[0]!.time
    const end = steps[steps.length - 1]!.time
    const total = Math.max(end - t0, 1)
    let previous = t0
    const rows = steps.map((e) => {
        const milestone = e.command < 0
        const name = milestone ? e.what : props.nameOf(0, e.command)
        const row = {
            key: `${e.time}:${name}`, milestone: milestone, label: milestone ? name : short(name),
            at: e.time - t0, from: ((previous - t0) / total) * 100, width: Math.max(0.4, ((e.time - previous) / total) * 100),
            color: milestone ? "#e6e6ee" : categoryOf(name).color,
        }
        previous = e.time
        return row
    })
    return { rows: rows, total: end - t0, done: steps.some((e) => e.what === "In the world") }
})
const time = (ms: number) => (ms >= 1000 ? `${(ms / 1000).toFixed(2)} s` : `${Math.round(ms)} ms`)
</script>

<template>
    <div class="join">
        <div v-if="!join" class="empty">
            <p>No join recorded yet.</p>
            <p class="faint">It's kept from when the game connects to a world.</p>
        </div>
        <template v-else>
            <p class="sum"><b>{{ time(join.total) }}</b> from connecting to {{ join.done ? "in the world" : "the last step so far" }}</p>
            <ol>
                <li v-for="r in join.rows" :key="r.key" :class="{ milestone: r.milestone }">
                    <span class="label"><i :style="{ background: r.color }" />{{ r.label }}</span>
                    <span class="at">{{ time(r.at) }}</span>
                    <span class="track"><span class="bar" :style="{ left: `${r.from}%`, width: `${r.width}%`, background: r.color }" /></span>
                </li>
            </ol>
        </template>
    </div>
</template>

<style scoped>
.join { height: 100%; overflow-y: auto; font-size: 12px; }
.sum { margin: 0; padding: 8px 10px; border-bottom: 1px solid var(--dt-line); color: var(--dt-dim); }
.sum b { color: var(--dt-ink); font-family: ui-monospace, monospace; font-weight: 500; }
ol { margin: 0; padding: 4px 0; list-style: none; }
li { display: grid; grid-template-columns: minmax(0, 190px) 64px minmax(0, 1fr); align-items: center; gap: 8px; height: 20px; padding: 0 10px; }
li:hover { background: var(--dt-hover); }
.label { display: flex; align-items: center; gap: 6px; min-width: 0; overflow: hidden; color: var(--dt-dim); white-space: nowrap; text-overflow: ellipsis; }
.label i { flex: none; width: 8px; height: 8px; border-radius: 2px; }
.milestone .label { color: var(--dt-ink); font-weight: 600; }
.at { color: var(--dt-faint); font: 11px ui-monospace, monospace; text-align: right; }
.track { position: relative; height: 8px; border-radius: 2px; background: rgb(255 255 255 / 0.04); }
.bar { position: absolute; top: 0; bottom: 0; border-radius: 2px; opacity: 0.85; }
.milestone .bar { opacity: 1; }
.empty { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 32px 16px; color: var(--dt-dim); text-align: center; }
.empty p { margin: 0; }
.faint { color: var(--dt-faint); }
</style>
