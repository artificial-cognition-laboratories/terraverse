import { onMounted, onUnmounted, ref, watch } from "vue"

/**
 * Where a live window ends: the engine's now, run on the page's own clock and
 * kept a little behind the newest frame, so it slides a little every page
 * frame. Ended at the newest frame, it jumped when a message came (about 27 a
 * second, out of step with the page's frames): a stutter however fast the
 * charts drew.
 */
const BEHIND_MS = 80     // under the newest frame: the next message is there before the window reaches it
const EASE = 0.05        // of the clock's drift taken up each message (never a jump)
const RESYNC_MS = 500    // off by more than this (paused, a stall): taken at once

export function smoothEnd(newest: () => number | undefined) {
    const end = ref(newest() ?? 1)
    let offset: number | null = null // the engine's ms less the page's
    let frame: number | null = null

    watch(newest, (last) => {
        if (last === undefined) return
        const now = performance.now()
        const target = last - now
        offset = offset === null || Math.abs(target - offset) > RESYNC_MS ? target : offset + (target - offset) * EASE
    })

    const tick = () => {
        frame = requestAnimationFrame(tick)
        const last = newest()
        if (offset === null || last === undefined) return
        // never past what's arrived
        end.value = Math.min(performance.now() + offset - BEHIND_MS, last)
    }
    onMounted(() => (frame = requestAnimationFrame(tick)))
    onUnmounted(() => frame !== null && cancelAnimationFrame(frame))
    return end
}
