<script setup lang="ts">
/**
 * The messages themselves, every one: when, which way, what, on which
 * channel, how big, and what it's about where its head says (a block's
 * position, an hp). Live, they stream in as they cross (asked for twice a
 * second), the newest followed until you scroll up; a capture holds a few
 * seconds still. Narrowed by the filter and the type picked in Messages.
 * Select one for its detail: every field, and the head of its payload as bytes.
 */
import { computed, onUnmounted, ref } from "vue"
import DataTable from "../../table/DataTable.vue"
import type { Column } from "../../table/types.ts"
import { formatBytes, short } from "./categories.ts"
import { bytesOf, summarise } from "./decode.ts"
import type { NetStoreHandle, Packet } from "./store.ts"

const props = defineProps<{ net: NetStoreHandle; text: string; picked: string | null; now: () => number | null }>()

const capture = computed(() => props.net.packets.value)
const live = props.net.live
const asking = setInterval(() => {
    const now = props.now()
    if (now !== null && !capture.value) void props.net.tail(now)
}, 500)
onUnmounted(() => clearInterval(asking))

const filter = computed(() => {
    const needle = props.text.trim().toLowerCase()
    const picked = props.picked
    return (p: Packet) => (!picked || picked === `${p.dir}:${p.command}`) && (!needle || p.name.toLowerCase().includes(needle))
})
const shown = ref(0)
const selected = ref<Packet | null>(null)

// captured: from the capture's start; live: from the oldest kept
const origin = computed(() => capture.value?.from ?? live.snapshot()[0]?.time ?? 0)
const ms = (p: Packet) => p.time - origin.value
const columns: Column<Packet>[] = [
    { id: "time", header: "Time", width: 76, mono: true, muted: true, value: (p) => (ms(p) >= 1000 ? `${(ms(p) / 1000).toFixed(3)} s` : `${ms(p).toFixed(1)} ms`) },
    { id: "dir", header: "", width: 28, value: (p) => (p.dir === 0 ? "↓" : "↑"), color: (p) => (p.dir === 0 ? "var(--dt-accent)" : "#e5a33b") },
    { id: "type", header: "Type", flex: 1, minWidth: 130, value: (p) => short(p.name), swatch: (p) => p.category.color },
    { id: "about", header: "About", flex: 1, minWidth: 120, muted: true, value: (p) => summarise(p.name, p.head) ?? "" },
    { id: "size", header: "Size", width: 66, mono: true, value: (p) => formatBytes(p.bytes) },
    { id: "channel", header: "Ch", width: 38, mono: true, muted: true, value: (p) => String(p.channel) },
]

const hexRows = computed(() => {
    const p = selected.value
    if (!p?.head) return []
    const bytes = [...bytesOf(p.head)]
    const rows: string[] = []
    for (let i = 0; i < bytes.length; i += 8) rows.push(bytes.slice(i, i + 8).map((b) => b.toString(16).padStart(2, "0")).join(" "))
    return rows
})
</script>

<template>
    <div class="packets">
        <div class="list">
            <div class="mode">
                <template v-if="capture">
                    <span>Captured {{ ((capture.to - capture.from) / 1000).toFixed(1) }} s</span>
                    <button @click="net.release()">Back to live</button>
                </template>
                <template v-else>
                    <span><i class="dot" :class="{ paused: live.state.paused }" />{{ live.state.paused ? `Paused · +${live.state.buffered}` : "Live" }}</span>
                    <button :class="{ on: live.state.following }" title="Follow the newest" @click="live.setFollowing(!live.state.following)">Follow</button>
                    <button @click="live.setPaused(!live.state.paused)">{{ live.state.paused ? "Resume" : "Pause" }}</button>
                    <button @click="live.clear()">Clear</button>
                </template>
                <span class="count">{{ shown.toLocaleString() }} / {{ (capture ? capture.list.length : live.state.total).toLocaleString() }}</span>
            </div>
            <DataTable v-if="capture" :key="'captured'" class="table" :columns="columns" :rows="capture.list" :filter="filter" :row-key="(p) => String(p.i)"
                @select="(p) => (selected = selected?.i === p.i ? null : p)" @filtered="(n) => (shown = n)" />
            <DataTable v-else :key="'live'" class="table" :columns="columns" :feed="live" :filter="filter" :row-key="(p) => String(p.i)"
                @select="(p) => (selected = selected?.i === p.i ? null : p)" @filtered="(n) => (shown = n)" />
        </div>
            <aside v-if="selected" class="detail">
                <header>
                    <i :style="{ background: selected.category.color }" />
                    <b>{{ short(selected.name) }}</b>
                    <button title="Close" aria-label="Close" @click="selected = null">×</button>
                </header>
                <dl>
                    <dt>Type</dt><dd>{{ selected.name }} <span class="faint">(0x{{ selected.command.toString(16).padStart(2, "0") }})</span></dd>
                    <dt>Way</dt><dd>{{ selected.dir === 0 ? "↓ from the server" : "↑ to the server" }}</dd>
                    <dt>When</dt><dd>{{ ms(selected).toFixed(2) }} ms into the capture</dd>
                    <dt>Size</dt><dd>{{ selected.bytes.toLocaleString() }} bytes</dd>
                    <dt>Channel</dt><dd>{{ selected.channel }} · {{ selected.reliable ? "reliable" : "unreliable" }}</dd>
                    <dt>Category</dt><dd>{{ selected.category.label }}</dd>
                    <template v-if="summarise(selected.name, selected.head)"><dt>About</dt><dd>{{ summarise(selected.name, selected.head) }}</dd></template>
                </dl>
                <template v-if="hexRows.length">
                    <h4>Payload, first {{ hexRows.join(" ").split(" ").length }} bytes</h4>
                    <pre>{{ hexRows.join("\n") }}</pre>
                </template>
                <p v-else class="faint">An outgoing message's payload isn't kept.</p>
            </aside>
    </div>
</template>

<style scoped>
.packets { position: relative; display: flex; height: 100%; min-height: 0; }
.list { display: flex; flex-direction: column; flex: 1; min-width: 0; }
.table { flex: 1; min-height: 0; }
.mode { display: flex; align-items: center; gap: 4px; flex: none; height: 26px; padding: 0 8px; border-bottom: 1px solid var(--dt-line); font-size: 11px; color: var(--dt-dim); }
.mode > span:first-child { display: flex; align-items: center; gap: 6px; margin-right: 6px; color: var(--dt-ink); }
.mode button { height: 20px; padding: 0 7px; border: 0; border-radius: 3px; background: none; color: var(--dt-dim); font: inherit; cursor: default; }
.mode button:hover { color: var(--dt-ink); background: var(--dt-hover); }
.mode button.on { color: var(--dt-accent); }
.dot { width: 7px; height: 7px; border-radius: 50%; background: #5fd17a; box-shadow: 0 0 5px #5fd17a; }
.dot.paused { background: var(--dt-faint); box-shadow: none; }
.detail { flex: none; width: 250px; overflow-y: auto; border-left: 1px solid var(--dt-line); background: var(--dt-rail); font-size: 12px; }
.detail header { display: flex; align-items: center; gap: 6px; height: 28px; padding: 0 6px 0 10px; border-bottom: 1px solid var(--dt-line); }
.detail header i { width: 8px; height: 8px; border-radius: 2px; }
.detail header b { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-weight: 600; }
.detail header button { border: 0; background: none; color: var(--dt-dim); font-size: 16px; cursor: default; }
.detail header button:hover { color: var(--dt-ink); }
dl { display: grid; grid-template-columns: auto 1fr; gap: 4px 10px; margin: 0; padding: 10px; }
dt { color: var(--dt-faint); }
dd { margin: 0; min-width: 0; overflow-wrap: anywhere; user-select: text; }
h4 { margin: 4px 10px 4px; font-size: 11px; font-weight: 600; color: var(--dt-dim); }
pre { margin: 0 10px 10px; padding: 6px 8px; border-radius: 4px; background: var(--dt-bg); font: 11px/1.5 ui-monospace, monospace; color: var(--dt-ink); user-select: text; }
.faint { color: var(--dt-faint); }
p.faint { padding: 0 10px; }
.count { margin-left: auto; color: var(--dt-faint); font: 10px ui-monospace, monospace; }
</style>
