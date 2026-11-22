<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { BridgeError, type Friend, type Loading, type Player, type PublicServer, type World } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import PixelIcon from "../game/PixelIcon.vue"
import { useStore } from "../game/store.ts"
import { Button, Dock, Notice, onEscape } from "../kit/index.ts"
import { usePlanetarium } from "../planetarium/use.ts"
import OnlineList from "./OnlineList.vue"
import { play as about, type Entry, type EntryGroup } from "./play/entries.ts"
import WorldDetails from "./play/WorldDetails.vue"
import WorldList from "./play/WorldList.vue"
import WorldStage from "./play/WorldStage.vue"
import type { Screen } from "./screens.ts"
import { chipsOf, GEAR, RAIL, type Section } from "./worldIcons.ts"

/**
 * Play: a dock of glass down the left (your worlds, friends' open ones, public servers),
 * and the chosen one over its planet in the room the dock leaves, with Play.
 */
// entering: the world chosen on its way (src/shell/entering.h; the menu's locked
// meanwhile); error: why the last one wasn't, or ended
const props = defineProps<{ entering: Loading | null; error: string | null }>()
const emit = defineEmits<{ go: [screen: Screen]; dismiss: [] }>()
const bridge = useBridge()
const store = useStore()
const section = ref<Section>("mine")
const query = ref("")
const worlds = ref<World[] | null>(null)
const selected = ref<string | null>(null)
const details = ref(false) // the selected world's own page, in the dock
// Esc steps back as Back does: out of a world's page, then home; never while a world is on its way
onEscape(() => { if (props.entering) return; if (details.value) details.value = false; else emit("go", "home") })
const busy = ref(false)
const problem = ref<string | null>(null)
const server = ref<PublicServer | null>(null) // the chosen public server, under Online

const orderedWorlds = computed(() => [...(worlds.value ?? [])].sort((a, b) =>
    Number(b.pinned) - Number(a.pinned) || (b.lastOpened ?? 0) - (a.lastOpened ?? 0) || a.name.localeCompare(b.name)))
const localEntry = (world: World): Entry => ({
    kind: "local", id: world.path, label: world.name,
    search: `${world.name} ${world.mode ?? ""} ${world.rules.tf_difficulty ?? ""} ${world.code ?? ""}`,
    world,
})
const friendGroups = computed<EntryGroup[]>(() => {
    const view = store.social?.view
    if (!view) return []
    const inviteHosts = new Set(view.invites.map((invite) => invite.from.id))
    const invites: Entry[] = view.invites.map((invite) => ({
        kind: "friend", id: `invite:${invite.id}`, label: invite.world,
        note: `Invited by ${invite.from.name || invite.from.username}`, tag: "Invite",
        search: `${invite.world} ${invite.from.username} ${invite.from.name ?? ""}`,
        player: invite.from, world: invite.world, invite,
    }))
    const open: Entry[] = view.friends.filter((friend) =>
        friend.presence?.state === "hosting" && friend.presence.open && !inviteHosts.has(friend.id))
        .sort((a, b) => a.username.localeCompare(b.username))
        .map((friend: Friend) => ({
            kind: "friend", id: `friend:${friend.id}`, label: friend.presence!.world || friend.username,
            note: `Hosted by ${friend.name || friend.username}`, tag: "Open",
            search: `${friend.presence!.world ?? ""} ${friend.username} ${friend.name ?? ""}`,
            player: friend, world: friend.presence!.world || "Their world",
        }))
    return [{ label: "Invitations", entries: invites }, { label: "Open worlds", entries: open }]
})
const groups = computed<EntryGroup[]>(() => {
    const raw: EntryGroup[] = section.value === "mine"
        ? [
            { label: "Pinned", entries: orderedWorlds.value.filter((w) => w.pinned).map(localEntry) },
            { label: "Recent", entries: orderedWorlds.value.filter((w) => !w.pinned && w.lastOpened !== null).map(localEntry) },
            { label: "New worlds", entries: orderedWorlds.value.filter((w) => !w.pinned && w.lastOpened === null).map(localEntry) },
        ]
        : section.value === "friends" ? friendGroups.value : []
    const term = query.value.trim().toLowerCase()
    return raw.map((group) => ({ ...group, entries: group.entries.filter((entry) => entry.search.toLowerCase().includes(term)) }))
        .filter((group) => group.entries.length)
})
const entries = computed(() => groups.value.flatMap((group) => group.entries))
const current = computed(() => entries.value.find((entry) => entry.id === selected.value) ?? null)
const local = computed(() => current.value?.kind === "local" ? current.value.world : null)
const friend = computed(() => current.value?.kind === "friend" ? current.value : null)

// what the stage shows over the planet: the chosen world, friend's or server
const shown = computed(() => {
    if (local.value) {
        const w = local.value, size = about.planetSize(w)
        return { kicker: w.pinned ? "Pinned world" : "Your world", title: w.name, chips: chipsOf(w),
            meta: [w.lastOpened ? `Last played ${about.date(w.lastOpened)}` : "Not played yet", size && `${size} block radius`].filter(Boolean).join(" · ") }
    }
    if (friend.value) return { kicker: friend.value.tag === "Invite" ? "Invitation" : "Friend's world", title: friend.value.label, chips: [], meta: friend.value.note }
    if (section.value === "online" && server.value)
        return { kicker: "Public server", title: server.value.name, chips: [], meta: `${server.value.players}/${server.value.capacity} playing · ${server.value.region}` }
    return { kicker: "", title: null, chips: [], meta: null }
})
const problems = computed(() => [props.error && `Couldn't play: ${props.error}`, problem.value].filter((p): p is string => !!p))

// the world selected is the universe behind: its system, framed by the Play
// shot (menu/screens.ts) in what the dock leaves (src/shell/planetarium.h)
const covers = ref({ left: 0, right: 0 })
const code = computed(() => local.value?.code ?? null)
const planet = usePlanetarium({
    bridge: bridge,
    place: "whole",
    active: computed(() => code.value !== null),
    body: computed(() => ({ code: code.value ?? "" })),
    frame: covers,
})
watch(section, () => { query.value = ""; selected.value = null; details.value = false })
watch(entries, (items) => {
    if (!items.some((item) => item.id === selected.value)) selected.value = items[0]?.id ?? null
}, { immediate: true })
watch(selected, () => { problem.value = null })

function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function refreshWorlds() {
    // (kept where the game's palette can see them: its #, where the menu isn't there to ask)
    try { worlds.value = store.worlds = await bridge.send("worlds.list", {}) }
    catch (err) { report(err) }
}
async function action(run: () => Promise<unknown>) {
    if (busy.value) return
    busy.value = true
    problem.value = null
    try { await run() }
    catch (err) { report(err) }
    finally { busy.value = false }
}
const play = (world: World) => action(() => bridge.send("play", { path: world.path }))
const joinServer = (s: PublicServer) => action(() => bridge.send("servers.join", { id: s.id }))
const join = (player: Player) => action(() => bridge.send("social.join", { id: player.id, name: player.username }))
const signIn = () => action(() => bridge.send("account.signIn", {}))
// a double click is the short way: most visits are pick a world and go
function activate(entry: Entry) {
    if (entry.kind === "local") void play(entry.world)
    else void join(entry.player)
}
function dismiss() {
    problem.value = null
    emit("dismiss")
}
async function removed() {
    details.value = false
    await refreshWorlds()
}
const orbits = (shown: boolean) => bridge.send("planetarium.orbits", { shown }).catch(report)
// a world that wasn't entered: the menu's host is new, and has none of what it was told
watch(() => store.mode, (mode, was) => {
    if (mode !== "menu" || was !== "entering") return
    void orbits(false)
    planet.again()
})
onMounted(() => {
    void orbits(false)
    void refreshWorlds()
})
onBeforeUnmount(() => { void orbits(true) })
</script>

<template>
    <div class="play-page">
        <Dock v-model:group="section" v-model:query="query" v-model:covers="covers"
            title="Play" :section="details && local ? local.name : ''" :groups="RAIL" rail-label="Places to play"
            :search-label="details ? undefined : 'Search worlds'" search-placeholder="Search worlds…"
            back="Back" @back="details ? details = false : emit('go', 'home')">

            <template v-if="details && local" #workspace>
                <WorldDetails :world="local" @changed="refreshWorlds" @removed="removed" />
            </template>

            <template v-else #list>
                <OnlineList v-if="section === 'online'" v-model="server" :query="query" @join="joinServer" />
                <div v-else-if="section === 'mine' && worlds === null" class="list-message">Reading your worlds…</div>
                <div v-else-if="section === 'friends' && store.account?.status === 'signedIn' && !store.social?.view" class="list-message">Finding friends' worlds…</div>
                <WorldList v-else-if="groups.length" v-model="selected" :groups="groups" @activate="activate" />
                <div v-else class="list-message">
                    <template v-if="query">No matches.</template>
                    <template v-else-if="section === 'mine'">No worlds yet. Make one to start.</template>
                    <template v-else-if="store.account?.status === 'signedOut'">
                        <p>Connect your ArcLabs account to see invitations and worlds your friends have opened.</p>
                        <Button size="sm" variant="primary" :disabled="busy" @click="signIn()">Connect ArcLabs »</Button>
                    </template>
                    <template v-else>When a friend opens a world or invites you, it appears here.</template>
                </div>
                <div v-if="section === 'friends' && store.social?.problem" class="list-message"><Notice tone="warn">{{ store.social.problem }}</Notice></div>
            </template>

            <template v-if="section === 'mine' && !details" #footer>
                <Button @click="emit('go', 'create')">+ New world</Button>
            </template>
        </Dock>

        <WorldStage :left="covers.left" :kicker="shown.kicker" :title="shown.title" :chips="shown.chips" :meta="shown.meta"
            :entering="entering" :problems="problems" @dismiss="dismiss">
            <template v-if="local">
                <Button variant="primary" size="lg" :disabled="busy" :busy="!!entering" @click="play(local)">Play »</Button>
                <Button v-if="!details && !entering" size="lg" aria-label="World settings" title="World settings" @click="details = true">
                    <PixelIcon class="gear" v-bind="GEAR" />
                </Button>
            </template>
            <Button v-else-if="friend" variant="primary" size="lg" :disabled="busy" :busy="!!entering" @click="join(friend.player)">Join »</Button>
            <Button v-else-if="section === 'online' && server" variant="primary" size="lg" :disabled="busy || !server.compatible" :busy="!!entering" @click="joinServer(server)">Join »</Button>
        </WorldStage>
    </div>
</template>

<style scoped>
.gear { width: 20px; height: 20px; }
.list-message { display: grid; gap: 0.6rem; justify-items: start; padding: 1rem 0.65rem; color: var(--ink-dim); font: 0.82rem/1.5 var(--font-sans); }
.list-message p { margin: 0; }
</style>
