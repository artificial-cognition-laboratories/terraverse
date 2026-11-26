<script setup lang="ts">
import PixelIcon from "../../game/PixelIcon.vue"
import { chipsOf, PIN, planetIcon } from "../worldIcons.ts"
import { play, type Entry, type EntryGroup } from "./entries.ts"

/** The Play list's groups of worlds: the chosen one lit along its edge; a double click goes straight in. */
defineProps<{ groups: readonly EntryGroup[] }>()
const selected = defineModel<string | null>({ required: true })
const emit = defineEmits<{ activate: [entry: Entry] }>()
</script>

<template>
    <section v-for="group in groups" :key="group.label" class="list-group">
        <h3>{{ group.label }}</h3>
        <button v-for="entry in group.entries" :key="entry.id" type="button" class="entry" :class="{ on: selected === entry.id }"
            :aria-current="selected === entry.id ? 'true' : undefined"
            @click="selected = entry.id" @dblclick="emit('activate', entry)">
            <template v-if="entry.kind === 'local'">
                <PixelIcon class="thumb" v-bind="planetIcon(entry.world.code ?? entry.world.path)" />
                <span class="copy">
                    <span class="line"><strong>{{ entry.label }}</strong>
                        <PixelIcon v-if="entry.world.pinned" class="pin" v-bind="PIN" aria-label="Pinned" /></span>
                    <span class="chips">
                        <span v-for="chip in chipsOf(entry.world)" :key="chip.label" class="chip" :title="chip.label">
                            <PixelIcon class="chip-icon" :rows="chip.rows" :palette="chip.palette" />{{ chip.text }}
                        </span>
                    </span>
                    <small>{{ entry.world.lastOpened ? play.date(entry.world.lastOpened) : "Not played yet" }}<template v-if="play.planetSize(entry.world)"> · {{ play.planetSize(entry.world) }} radius</template></small>
                </span>
            </template>
            <template v-else-if="entry.kind === 'server'">
                <PixelIcon class="thumb" v-bind="planetIcon(entry.server.ref)" />
                <span class="copy">
                    <strong>{{ entry.label }}</strong>
                    <span class="chips">
                        <span class="chip local">Local</span>
                        <span v-for="mod in entry.server.mods.slice(0, 2)" :key="mod" class="chip">{{ mod }}</span>
                    </span>
                    <small>{{ entry.server.ref }}</small>
                </span>
            </template>
            <template v-else-if="entry.kind === 'address'">
                <PixelIcon class="thumb" v-bind="planetIcon(entry.ref)" />
                <span class="copy"><strong>{{ entry.label }}</strong><small>Join this address</small></span>
            </template>
            <template v-else>
                <span class="avatar">{{ entry.player.username.slice(0, 1).toUpperCase() }}</span>
                <span class="copy"><strong>{{ entry.label }}</strong><small>{{ entry.note }}</small></span>
                <span class="tag">{{ entry.tag }}</span>
            </template>
        </button>
    </section>
</template>

<style scoped>
.list-group { margin: 0.75rem 0 1rem; }
h3 { margin: 0 0 0.4rem; padding: 0 0.65rem; color: var(--ink-faint); font: 400 0.62rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; }
.entry { display: grid; grid-template-columns: 40px minmax(0, 1fr) auto; align-items: center; gap: 0.75rem; width: 100%; min-width: 0; padding: 0.6rem 0.65rem; border: 0; background: none; box-shadow: inset 2px 0 0 transparent; color: var(--ink); text-align: left; transition: background 0.1s; }
.entry:hover { background: rgb(231 237 244 / 0.04); }
.entry.on { background: rgb(99 163 233 / 0.07); box-shadow: inset 2px 0 0 var(--accent-hi); }
.thumb { width: 40px; height: 40px; }
.copy { display: flex; flex-direction: column; gap: 0.3rem; min-width: 0; }
.line { display: flex; align-items: center; gap: 0.4rem; min-width: 0; }
strong { overflow: hidden; font: 400 0.75rem var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; text-overflow: ellipsis; white-space: nowrap; }
.entry.on strong { color: var(--accent-hi); }
small { overflow: hidden; color: var(--ink-faint); font-size: 0.68rem; text-overflow: ellipsis; white-space: nowrap; }
.pin { flex: none; width: 11px; height: 11px; }
.chips { display: flex; flex-wrap: wrap; gap: 0.2rem 0.6rem; min-width: 0; }
.chip { display: inline-flex; align-items: center; gap: 0.3rem; color: var(--ink-dim); font: 0.58rem var(--font-display); text-transform: uppercase; white-space: nowrap; }
.chip.local { color: var(--accent-hi); }
.chip-icon { width: 11px; height: 11px; }
.avatar { display: grid; place-items: center; width: 40px; height: 40px; border: 1px solid var(--glass-edge-hi); border-radius: var(--radius); background: rgb(10 75 145 / 0.5); font: 1rem var(--font-display); }
.tag { color: var(--accent-hi); font: 0.58rem var(--font-display); text-transform: uppercase; }
</style>
