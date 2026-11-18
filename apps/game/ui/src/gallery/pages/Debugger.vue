<script setup lang="ts">
import { onUnmounted, ref } from "vue"
import type { DockSide, LogLevel, LogLine } from "../../bridge/index.ts"
import { TableFeed } from "../../table/feed.ts"
import { MOCK_NAMES, MOCK_UNITS, mockCapture, mockEvents, mockFrame, mockMessages } from "../../debugger/perf/mock.ts"
import { NetStore } from "../../debugger/net/store.ts"
import { PerfStore } from "../../debugger/perf/store.ts"
import DebuggerPanel from "../../debugger/DebuggerPanel.vue"
import Specimen from "../Specimen.vue"

/** The debugger's page as the engine sizes it beside the game; its dock button switches this frame's side. */
const side = ref<DockSide>("right")

// A log as the engine would stream it: a burst at start, then a line now and then.
const log = TableFeed<LogLine>({ key: (line) => String(line.seq), max: 20_000 })
const SAMPLES: [LogLevel, string, string][] = [
    ["info", "Main", "shell: loading terra://ui/index.html"],
    ["action", "Server", "singleplayer joins game. List of players: singleplayer"],
    ["warning", "Main", "Irrlicht: Could not open file of texture: tf_world_unknown.png"],
    ["info", "Emerge-0", "terra: generated 64 blocks in 41 ms"],
    ["error", "Server", "ModError: tf_craft/init.lua:212: attempt to index a nil value (field 'station')"],
    ["info", "Main", "far: 1296 tiles, view range 120"],
]
let seq = 0
const line = (): LogLine => {
    const sample = SAMPLES[seq % SAMPLES.length]
    if (!sample) throw new Error("gallery: no log samples")
    return { seq: ++seq, time: Date.now(), level: sample[0], thread: sample[1], text: sample[2] }
}
log.append(Array.from({ length: 400 }, line))
const ticking = setInterval(() => log.append([line()]), 700)

// Frames as the recorder streams them, ten times a second.
const perf = PerfStore({ capture: async (from, to) => mockCapture(perf.frames.value, from, to) })
let frameId = 0
let clock = 0
const frames = (n: number) => Array.from({ length: n }, () => {
    const frame = mockFrame(++frameId, clock)
    clock = frame.end
    return frame
})
perf.append(frames(600), MOCK_UNITS)
const net = NetStore({ capture: async (from, to) => mockMessages(perf.frames.value, from, to) })
net.names.value = MOCK_NAMES
net.addEvents(mockEvents(perf.frames.value[perf.frames.value.length - 1]?.end ?? 0))
const streaming = setInterval(() => perf.append(frames(6)), 100)
const exportTrace = async () => "/home/you/.terraverse/traces/trace-1795000000.json"
onUnmounted(() => {
    clearInterval(ticking)
    clearInterval(streaming)
})
</script>

<template>
    <Specimen name="Debugger" note="devtools dress, not the kit: it's a tool beside the game" wide>
        <div class="window" :class="side">
            <div class="game">game</div>
            <DebuggerPanel :side="side" :log="log" :perf="perf" :net="net" :export-trace="exportTrace" class="panel" @dock="(to) => side = to" />
        </div>
    </Specimen>
</template>

<style scoped>
.window { display: flex; width: 100%; height: 560px; border: var(--b) solid var(--edge); background: #060d16; }
.window.bottom { flex-direction: column; }
.game { flex: 1; display: grid; place-items: center; color: var(--ink-faint); min-width: 0; min-height: 0; }
.right .panel { width: 460px; }
.bottom .panel { height: 260px; }
</style>
