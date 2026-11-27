<script setup lang="ts">
import { computed, ref } from "vue"
import type { PublicServer } from "../bridge/index.ts"
import { useCache } from "../cache/index.ts"
import { Button, DetailHeader, DetailLayout, ICONS, ListEmpty, Notice, SideCard, SideNav, SideStats } from "../kit/index.ts"
import { format } from "../menu/library/format.ts"
import type { ActionsHandle } from "./actions.ts"

/**
 * A public server's own page, laid out as a mod's: its banner with the address to copy, its name,
 * version and join at the end of the title line, what it is, then its mods or who's on it beside
 * its numbers. Read from the directory's list (there's no page per server to fetch); a server gone
 * from it says so.
 */
type Tab = "about" | "mods" | "players"
const props = defineProps<{ id: string; back: string; actions: ActionsHandle }>()
const emit = defineEmits<{ back: []; mod: [name: string] }>()
const directory = useCache().cached("servers.list")
const server = computed<PublicServer | null>(() => directory.data.value?.servers.find((s) => s.id === props.id) ?? null)
const tab = ref<Tab>("about")
const tabs = computed(() => [
    { id: "about" as const, label: "About", icon: ICONS.readme },
    { id: "mods" as const, label: "Mods", icon: ICONS.mods, count: server.value?.mods.length ?? null },
    { id: "players" as const, label: "Players", icon: ICONS.players, count: server.value?.players ?? null },
])
const stats = computed(() => server.value ? [
    { icon: ICONS.players, value: `${server.value.players}/${server.value.capacity}`, label: "playing" },
    { icon: ICONS.region, value: server.value.pingMs === null ? "—" : `${server.value.pingMs}`, label: "ms" },
    { icon: ICONS.mods, value: String(server.value.mods.length), label: "mods" },
] : [])
const full = computed(() => !!server.value && server.value.players >= server.value.capacity)
const gone = computed(() => directory.data.value && !server.value ? "This server isn't in the directory any more." : directory.problem.value)
const favorite = async (s: PublicServer) => {
    if (await props.actions.send("servers.favorite.set", { id: s.id, favorite: !s.favorite })) await directory.refresh({})
}
</script>

<template>
    <DetailLayout :back="back" :ready="!!server" :problem="gone" @back="emit('back')">
        <template v-if="server">
            <DetailHeader :seed="server.id" :title="server.name" :address="server.ref" :description="server.description">
                <template #badges>
                    <span class="badge">v{{ server.version }}</span>
                    <span v-if="server.featured" class="badge lit">Featured</span>
                    <span v-if="!server.compatible" class="badge warn">Other version</span>
                </template>
                <template #actions>
                    <Button size="sm" :disabled="actions.busy.value" @click="favorite(server)">{{ server.favorite ? "★ Favourite" : "☆ Favourite" }}</Button>
                    <Button variant="primary" size="sm" :disabled="actions.busy.value || !server.compatible || full"
                        :title="!server.compatible ? 'It runs another version' : full ? 'It\'s full' : undefined"
                        @click="actions.joinServer(server)">{{ actions.joining.value === server.id ? "Joining…" : "Join »" }}</Button>
                </template>
                <template #by>hosted by <b>{{ server.host }}</b> · {{ server.region }}</template>
            </DetailHeader>
            <Notice v-if="actions.problem.value" tone="danger">{{ actions.problem.value }}</Notice>
            <section v-if="tab === 'about'" class="about">
                <p v-if="server.tags.length" class="tags"><span v-for="t in server.tags" :key="t">{{ t }}</span></p>
                <p class="words">{{ server.description || "Its host hasn't described it." }}</p>
            </section>
            <template v-else-if="tab === 'mods'">
                <ListEmpty v-if="!server.mods.length" title="No mods"><p>It plays the game as it comes.</p></ListEmpty>
                <ul v-else class="mods">
                    <li v-for="m in server.mods" :key="m"><button type="button" @click="emit('mod', m)"><code>{{ m }}</code><span>»</span></button></li>
                </ul>
            </template>
            <!-- unwired: the directory counts players, it doesn't name them -->
            <Notice v-else tone="info">{{ server.players }} playing now. Who's on a server isn't listed yet.</Notice>
        </template>
        <template #side>
            <SideCard v-if="server">
                <SideNav v-model="tab" :items="tabs" />
                <SideStats :items="stats" />
                <dl class="meta">
                    <div><dt>Address</dt><dd>{{ server.ref }}</dd></div>
                    <div><dt>Version</dt><dd>{{ server.version }}</dd></div>
                    <div><dt>Region</dt><dd>{{ server.region }}</dd></div>
                    <div><dt>Room</dt><dd>{{ format.count(server.capacity - server.players) }} free</dd></div>
                </dl>
            </SideCard>
        </template>
    </DetailLayout>
</template>

<style scoped>
.badge.warn { border-color: rgb(210 153 34 / 0.5); color: var(--warn); }
.about { display: grid; gap: 1rem; }
.tags { display: flex; flex-wrap: wrap; gap: 0.5rem; margin: 0; }
.tags span { padding: 0.25rem 0.55rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); color: var(--ink-dim); font: 0.6rem var(--font-display); text-transform: uppercase; }
.words { max-width: 44rem; margin: 0; color: var(--ink-dim); font: 0.95rem/1.6 var(--font-sans); white-space: pre-line; }
.mods { display: grid; gap: 0.5rem; margin: 0; padding: 0; list-style: none; }
.mods button { display: flex; justify-content: space-between; width: 100%; padding: 0.8rem 1rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); color: var(--ink); text-align: left; }
.mods button:hover { border-color: var(--edge-lit); color: var(--accent-hi); }
.meta { display: grid; gap: 0.45rem; margin: 0; padding: 1rem; font-size: 0.75rem; }
.meta div { display: flex; justify-content: space-between; gap: 1rem; }
dt { color: var(--ink-faint); }
dd { margin: 0; min-width: 0; overflow: hidden; color: var(--ink-dim); text-align: right; text-overflow: ellipsis; white-space: nowrap; }
</style>
