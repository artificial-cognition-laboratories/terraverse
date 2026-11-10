<script setup lang="ts">
import { computed, ref } from "vue"
import { Button, ListEmpty, ListGroup, ListItem, Notice, WorkbenchColumns } from "@game/kit/index.ts"
import PixelIcon from "@game/game/PixelIcon.vue"
import type { Channel } from "../backend/index.ts"
import { STACK } from "../icons.ts"
import Notes from "../parts/Notes.vue"
import PlayButton from "../parts/PlayButton.vue"
import { bytes, date, useStore } from "../use.ts"

const store = useStore()
const { state } = store

const picked = ref<string | null>(null)
const version = computed(() => store.visible.value.find(v => v.id === (picked.value ?? store.target.value?.id)) ?? null)
const standing = computed(() => version.value ? store.standing(version.value.id) : null)
const pinned = computed(() => state.settings?.pinned ?? null)
const running = computed(() => state.game?.state === "running" ? state.game.version : null)

const CHANNELS: { id: Channel; title: string }[] = [{ id: "stable", title: "Stable" }, { id: "beta", title: "Beta" }]

function trail(id: string): string {
    const s = store.standing(id)
    if (s.state === "downloading") return `${Math.round(s.progress.received / s.progress.total * 100)}%`
    if (s.state === "failed") return "failed"
    return s.state === "installed" ? "✓" : ""
}
</script>

<template>
    <WorkbenchColumns :list-width="280">
        <template #list>
            <ListEmpty v-if="store.visible.value.length === 0" title="No versions">Nothing's been released for this platform yet.</ListEmpty>
            <template v-for="channel in CHANNELS" :key="channel.id">
                <ListGroup v-if="store.visible.value.some(v => v.channel === channel.id)" :title="channel.title">
                    <ListItem v-for="v in store.visible.value.filter(v => v.channel === channel.id)" :key="v.id"
                        :on="v.id === version?.id" @click="picked = v.id">
                        {{ v.id }}
                        <template #sub>{{ date(v.released) }}{{ v.id === store.target.value?.id ? " · plays" : "" }}</template>
                        <template #trail>{{ trail(v.id) }}</template>
                    </ListItem>
                </ListGroup>
            </template>
        </template>

        <template #content>
            <article v-if="version && standing" class="page">
                <!-- like an extension's page: what it is, what you can do, then what's in it -->
                <header>
                    <div class="icon"><PixelIcon :rows="STACK.rows" :palette="STACK.palette" style="--u: 5px" /></div>
                    <div class="what">
                        <div class="title">
                            <h2>{{ version.id }}</h2>
                            <span class="tag" :class="version.channel">{{ version.channel }}</span>
                            <span v-if="pinned === version.id" class="tag pinned">pinned</span>
                        </div>
                        <p class="meta">
                            Released {{ date(version.released) }}
                            · {{ standing.state === "installed" ? `${bytes(standing.size)} on disk` : `${bytes(version.size)} download` }}
                            <template v-if="version.protocol !== null">· protocol {{ version.protocol }}</template>
                        </p>
                        <div class="actions">
                            <div class="play"><PlayButton :version="version" /></div>
                            <Button v-if="pinned === version.id" size="sm" @click="store.pin(null)">Unpin</Button>
                            <Button v-else size="sm" @click="store.pin(version.id)">Pin</Button>
                            <Button v-if="standing.state === 'installed'" size="sm" variant="danger"
                                :disabled="running === version.id" @click="store.remove(version.id)">Remove</Button>
                        </div>
                    </div>
                </header>
                <Notice v-if="standing.state === 'failed'" tone="danger">{{ standing.reason }}</Notice>
                <section>
                    <h3>Release notes</h3>
                    <Notes :text="version.notes" />
                </section>
                <p class="dim">Pinned, Play on the Game page launches this version instead of the newest. Worlds are shared by every version; opening one in a newer version offers a backup first.</p>
            </article>
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
.page { display: flex; flex-direction: column; gap: 24px; padding-top: 18px; }
header { display: flex; gap: 22px; padding-bottom: 22px; border-bottom: 1px solid var(--stone-4); }
.icon { display: grid; place-items: center; flex: none; width: 104px; height: 104px; background: var(--stone-2); border: var(--b) solid var(--edge); box-shadow: var(--raised); }
.what { display: flex; flex-direction: column; justify-content: center; gap: 8px; min-width: 0; min-height: 104px; }
.title { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
h2 { margin: 0; font: 400 24px var(--font-display); text-transform: uppercase; }
.tag { padding: 1px 6px; font: 10px var(--font-display); text-transform: uppercase; letter-spacing: 0.1em; border: 1px solid var(--edge); }
.tag.stable { background: var(--accent-lo); }
.tag.beta { background: var(--warn-lo); }
.tag.pinned { background: var(--stone-4); }
.meta { margin: 0; color: var(--ink-dim); font-size: 12px; }
.actions { display: flex; align-items: stretch; gap: 8px; flex-wrap: wrap; margin-top: 4px; }
.play { min-width: 200px; --play-height: 34px; }
.actions > :deep(.btn) { height: 34px; }
.play :deep(.go) { font-size: 13px; }
h3 { margin: 0 0 12px; font: 11px var(--font-display); text-transform: uppercase; letter-spacing: 0.12em; color: var(--ink-faint); }
.dim { margin: 0; color: var(--ink-faint); font-size: 11px; }
</style>
