<script setup lang="ts">
/**
 * The last join, as a waterfall: from "Connecting" to "In the world", each
 * step a row (the milestones strong, the first arrival of each message type
 * between them), its bar from the step before to it, so the long waits show
 * as long bars. The engine keeps these events, so the join is here however
 * long ago it was.
 */
import { computed } from "vue"
import type { NetEvent } from "../../bridge/index.ts"
import Waterfall, { type Step } from "../kit/Waterfall.vue"
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
    const events = list.slice(start).sort((a, b) => a.time - b.time)
    let previous = events[0]!.time
    const steps = events.map((e): Step => {
        const milestone = e.command < 0
        const name = milestone ? e.what : props.nameOf(0, e.command)
        const step = { key: `${e.time}:${name}`, label: milestone ? name : short(name), start: previous, end: e.time,
            color: milestone ? "#e6e6ee" : categoryOf(name).color, strong: milestone }
        previous = e.time
        return step
    })
    const total = events[events.length - 1]!.time - events[0]!.time
    const done = events.some((e) => e.what === "In the world")
    const time = total >= 1000 ? `${(total / 1000).toFixed(2)} s` : `${Math.round(total)} ms`
    return { steps: steps, summary: `${time} from connecting to ${done ? "in the world" : "the last step so far"}` }
})
</script>

<template>
    <Waterfall v-if="join" :steps="join.steps" :summary="join.summary" />
    <div v-else class="empty">
        <p>No join recorded yet.</p>
        <p class="faint">It's kept from when the game connects to a world.</p>
    </div>
</template>

<style scoped>
.empty { display: flex; flex-direction: column; align-items: center; gap: 4px; padding: 32px 16px; color: var(--dt-dim); font-size: 12px; text-align: center; }
.empty p { margin: 0; }
.faint { color: var(--dt-faint); }
</style>
