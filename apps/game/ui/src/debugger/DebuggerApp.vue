<script setup lang="ts">
import { onUnmounted, ref } from "vue"
import { useBridge } from "../bridge/use.ts"
import type { DockSide, LogLine } from "../bridge/index.ts"
import { TableFeed } from "../table/feed.ts"
import DebuggerPanel from "./DebuggerPanel.vue"
import { PerfStore } from "./perf/store.ts"
import { NetStore } from "./net/store.ts"

/**
 * The debugger's page: the panel, on whichever side the engine says (nothing
 * until it has), and the engine's log, kept here so it fills whatever tab is open.
 */
const bridge = useBridge()
const side = ref<DockSide | null>(null)
const log = TableFeed<LogLine>({ key: (line) => String(line.seq), max: 20_000 })
const perf = PerfStore({ capture: (from, to) => bridge.send("perf.capture", { from: from, to: to }) })
const net = NetStore({ capture: (from, to) => bridge.send("net.capture", { from: from, to: to }) })
const exportTrace = async () => (await bridge.send("perf.export", {})).path
const off = bridge.on((message) => {
    if (message.kind === "debugger.dock") side.value = message.side
    else if (message.kind === "debugger.log") log.append(message.lines)
    else if (message.kind === "perf.frames") perf.append(message.frames, message.units)
    else if (message.kind === "net.names") net.names.value = message.names
    else if (message.kind === "net.events") net.addEvents(message.events)
})
onUnmounted(off)
</script>

<template>
    <DebuggerPanel v-if="side" :side="side" :log="log" :perf="perf" :net="net" :export-trace="exportTrace" @dock="(to) => bridge.send('debugger.dock', { side: to })"
        @grab="bridge.send('debugger.drag', {})" @close="bridge.send('debugger.close', {})" />
</template>
