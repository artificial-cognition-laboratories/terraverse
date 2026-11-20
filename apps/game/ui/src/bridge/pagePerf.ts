import type { BridgeHandle } from "./index.ts"

/**
 * The page's own cost, for the engine's debugger ("UI: page …" counters):
 * once a second, its longest script task (Chromium's long tasks, 50 ms and
 * up), how long all of them took, and the longest gap between its animation
 * frames. Chromium paints only when the engine asks (external begin frames),
 * so a gap past a frame or two is the page's own work holding it up.
 */
export function PagePerf(opts: { bridge: BridgeHandle }) {
    let longest = 0
    let busy = 0
    let gap = 0
    let last = performance.now()

    const tasks = new PerformanceObserver((list) => {
        for (const entry of list.getEntries()) {
            longest = Math.max(longest, entry.duration)
            busy += entry.duration
        }
    })
    tasks.observe({ type: "longtask", buffered: false })

    const frame = (now: number) => {
        gap = Math.max(gap, now - last)
        last = now
        requestAnimationFrame(frame)
    }
    requestAnimationFrame(frame)

    const report = setInterval(() => {
        void opts.bridge.send("page.perf", { longestTask: longest, busy: busy, longestGap: gap })
        longest = 0
        busy = 0
        gap = 0
    }, 1000)

    return {
        stop() {
            tasks.disconnect()
            clearInterval(report)
        },
    }
}
