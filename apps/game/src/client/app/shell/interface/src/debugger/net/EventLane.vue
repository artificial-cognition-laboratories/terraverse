<script setup lang="ts">
/**
 * The connection's moments on the charts' time axis: connecting, content
 * ready, in the world, and the first arrival of each message type. A marker
 * each, labelled where there's room, named in full on hover.
 */
import { computed } from "vue"
import type { NetEvent } from "../../bridge/index.ts"
import type { Window } from "../perf/view.ts"
import { categoryOf, short } from "./categories.ts"

const props = defineProps<{ events: readonly NetEvent[]; window: Window; nameOf: (dir: 0 | 1, command: number) => string }>()

const shown = computed(() => {
    const { startMs, endMs } = props.window
    const span = endMs - startMs
    const inView = props.events.filter((e) => e.time >= startMs && e.time <= endMs)
    // Labels only where they fit beside the last: milestones placed first,
    // then the first arrivals in what room is left.
    const placed: number[] = []
    const room = (at: number) => placed.every((p) => Math.abs(at - p) > 16)
    const atOf = (time: number) => ((time - startMs) / span) * 100
    const labelled = new Set<number>()
    for (const pass of [true, false]) {
        inView.forEach((e, i) => {
            if ((e.command < 0) !== pass || !room(atOf(e.time))) return
            placed.push(atOf(e.time))
            labelled.add(i)
        })
    }
    return inView
        .map((e, i) => {
            const milestone = e.command < 0
            const name = milestone ? e.what : props.nameOf(0, e.command)
            const at = atOf(e.time)
            return {
                key: `${e.time}:${name}`, at: at, milestone: milestone, labelled: labelled.has(i),
                label: milestone ? name : short(name), title: milestone ? name : `First ${name}`,
                color: milestone ? "var(--dt-ink)" : categoryOf(name).color,
            }
        })
})
</script>

<template>
    <div class="lane">
        <b class="title">Events</b>
        <span v-for="e in shown" :key="e.key" class="mark" :class="{ milestone: e.milestone }" :style="{ left: `${e.at}%` }" :title="e.title">
            <i :style="{ background: e.color }" /><em v-if="e.labelled">{{ e.label }}</em>
        </span>
    </div>
</template>

<style scoped>
.lane { position: relative; height: 26px; border-bottom: 1px solid var(--dt-line); overflow: hidden; }
.title { position: absolute; top: 5px; left: 6px; z-index: 1; font-size: 11px; font-weight: 600; color: var(--dt-ink); }
.mark { position: absolute; top: 0; bottom: 0; display: flex; align-items: flex-end; gap: 3px; padding-bottom: 4px; pointer-events: auto; }
.mark i { width: 1px; height: 100%; opacity: 0.6; }
.mark.milestone i { width: 2px; opacity: 1; }
.mark em { position: absolute; left: 4px; bottom: 3px; font-style: normal; font-size: 10px; color: var(--dt-dim); white-space: nowrap; }
.mark.milestone em { color: var(--dt-ink); font-weight: 600; }
</style>
