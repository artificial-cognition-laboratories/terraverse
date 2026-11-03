<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { BridgeError, type CommunityCatalog, type Friend, type Invite, type Player, type PublicServer, type ServerDirectory, type World } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import { Button, Dock, Notice } from "../kit/index.ts"
import { usePlanetarium } from "../planetarium/use.ts"
import { decodeWorldCode } from "../worldgen/config.ts"
import { Copier } from "./create/copy.ts"
import type { Screen } from "./screens.ts"

type Section = "mine" | "friends" | "servers"
type Entry =
    | { kind: "local"; id: string; label: string; note: string; tag?: string; search: string; world: World }
    | { kind: "friend"; id: string; label: string; note: string; tag?: string; search: string; player: Player; world: string; invite?: Invite }
    | { kind: "server"; id: string; label: string; note: string; tag?: string; search: string; server: PublicServer }
type ListGroup = { label: string; entries: Entry[] }

const emit = defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()
const store = useStore()
const copier = Copier()
const section = ref<Section>("mine")
const query = ref("")
const worlds = ref<World[] | null>(null)
const catalog = ref<CommunityCatalog | null>(null)
const directory = ref<ServerDirectory | null>(null)
const selected = ref<string | null>(null)
const confirming = ref(false)
const busy = ref(false)
const problem = ref<string | null>(null)

const RAIL: { id: Section; label: string; shortLabel: string; rows: string[]; palette: Record<string, string> }[] = [
    { id: "mine", label: "My Worlds", shortLabel: "Mine", palette: { A: "#c9d4dc", B: "#40b8ec", G: "#5fae5a" },
        rows: ["...AAA...", ".AABBAA..", "AABBBBAA.", "ABGGGBBBA", "ABGGGBBBA", "AABBBBBA.", ".AABBAA..", "...AAA...", "........."] },
    { id: "friends", label: "Friends", shortLabel: "Friends", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["..AA.AA..", ".ABBABBA.", ".ABBBBA..", "..AAAAA..", ".AAAAAAA.", "AABBBBBAA", "ABBBBBBBA", "AAAAAAAAA", "........."] },
    { id: "servers", label: "Servers", shortLabel: "Servers", palette: { A: "#c9d4dc", B: "#40b8ec" },
        rows: ["..AAAAA..", ".ABBBBBA.", ".ABBBBBA.", ".AAAAAAA.", "..AAAAA..", ".ABBBBBA.", ".ABBBBBA.", ".AAAAAAA.", "..AAAAA.."] },
]
const sectionTitle = computed(() => RAIL.find((item) => item.id === section.value)!.label)

const orderedWorlds = computed(() => [...(worlds.value ?? [])].sort((a, b) =>
    Number(b.pinned) - Number(a.pinned) || (b.lastOpened ?? 0) - (a.lastOpened ?? 0) || a.name.localeCompare(b.name)))
const localEntry = (world: World): Entry => ({
    kind: "local", id: world.path, label: world.name,
    note: world.lastOpened ? `Opened ${date(world.lastOpened)}` : "Not played yet",
    tag: world.pinned ? "Pinned" : undefined,
    search: `${world.name} ${world.mode ?? ""} ${world.rules.tf_difficulty ?? ""} ${world.code ?? ""}`,
    world,
})
const friendGroups = computed<ListGroup[]>(() => {
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
const serverGroups = computed<ListGroup[]>(() => {
    const servers = directory.value?.servers ?? []
    const favorites = servers.filter((server) => server.favorite)
    const featured = servers.filter((server) => server.featured && !server.favorite)
    const others = servers.filter((server) => !server.featured && !server.favorite)
    const entry = (server: PublicServer): Entry => ({
        kind: "server", id: server.id, label: server.name, note: `${server.players}/${server.capacity} playing · ${server.region}`,
        tag: server.favorite ? "Saved" : server.featured ? "Featured" : undefined,
        search: `${server.name} ${server.description} ${server.host} ${server.region} ${server.tags.join(" ")}`,
        server,
    })
    return [
        { label: "Favorites", entries: favorites.map(entry) },
        { label: "Featured", entries: featured.map(entry) },
        { label: "All servers", entries: others.map(entry) },
    ]
})
const groups = computed<ListGroup[]>(() => {
    const raw: ListGroup[] = section.value === "mine"
        ? [
            { label: "Pinned", entries: orderedWorlds.value.filter((w) => w.pinned).map(localEntry) },
            { label: "Recent", entries: orderedWorlds.value.filter((w) => !w.pinned && w.lastOpened !== null).map(localEntry) },
            { label: "New worlds", entries: orderedWorlds.value.filter((w) => !w.pinned && w.lastOpened === null).map(localEntry) },
        ]
        : section.value === "friends" ? friendGroups.value : serverGroups.value
    const term = query.value.trim().toLowerCase()
    return raw.map((group) => ({ ...group, entries: group.entries.filter((entry) => entry.search.toLowerCase().includes(term)) }))
        .filter((group) => group.entries.length)
})
const entries = computed(() => groups.value.flatMap((group) => group.entries))
const current = computed(() => entries.value.find((entry) => entry.id === selected.value) ?? null)
const local = computed(() => current.value?.kind === "local" ? current.value.world : null)

// the world selected is the universe behind: its system, the camera flown to
// it (src/shell/planetarium.h; the menu's own when none is)
const covers = ref({ left: 0, right: 0 })
const code = computed(() => local.value?.code ?? null)
const planet = usePlanetarium({
    bridge: bridge,
    place: "whole",
    active: computed(() => code.value !== null),
    body: computed(() => ({ code: code.value ?? "" })),
    frame: covers,
})
watch(code, (now) => {
    if (now) planet.fly({ radii: 3, seconds: 2 })
})
const friend = computed(() => current.value?.kind === "friend" ? current.value : null)
const server = computed(() => current.value?.kind === "server" ? current.value.server : null)
const installedMods = computed(() => catalog.value?.mods.filter((mod) =>
    mod.source === "installed" && local.value && mod.enabledWorlds.includes(local.value.path)) ?? null)
const size = computed(() => {
    if (!local.value?.code) return null
    try { return decodeWorldCode(local.value.code).size }
    catch { return null }
})
watch(section, () => { query.value = ""; selected.value = null; confirming.value = false })
watch(entries, (items) => {
    if (!items.some((item) => item.id === selected.value)) selected.value = items[0]?.id ?? null
}, { immediate: true })
watch(selected, () => { confirming.value = false; problem.value = null })

function date(seconds: number) {
    return new Date(seconds * 1000).toLocaleDateString(undefined, { day: "numeric", month: "short", year: "numeric" })
}
function report(err: unknown) {
    if (!(err instanceof BridgeError && err.refused)) console.error(err)
    problem.value = err instanceof Error ? err.message : String(err)
}
async function refreshWorlds() {
    try { worlds.value = await bridge.send("worlds.list", {}) }
    catch (err) { report(err) }
}
async function refreshServers() {
    try { directory.value = await bridge.send("servers.list", {}) }
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
function play(world: World) { return action(() => bridge.send("play", { path: world.path })) }
function join(player: Player) { return action(() => bridge.send("social.join", { id: player.id, name: player.username })) }
function joinServer(item: PublicServer) { return action(() => bridge.send("servers.join", { id: item.id })) }
function signIn() { return action(() => bridge.send("account.signIn", {})) }
function pin(world: World) { return action(async () => {
    await bridge.send("worlds.pin", { path: world.path, pinned: !world.pinned })
    await refreshWorlds()
}) }
function favorite(item: PublicServer) { return action(async () => {
    await bridge.send("servers.favorite.set", { id: item.id, favorite: !item.favorite })
    await refreshServers()
}) }
function remove(world: World) { return action(async () => {
    await bridge.send("worlds.delete", { path: world.path })
    confirming.value = false
    await refreshWorlds()
}) }
onMounted(() => {
    void refreshWorlds()
    void refreshServers()
    void bridge.send("community.list", {}).then((value) => { catalog.value = value }).catch(report)
})
</script>

<template>
    <Dock v-model:group="section" v-model:query="query" v-model:covers="covers" class="play-panel"
        title="Play" :section="sectionTitle" :groups="RAIL" rail-label="Places to play"
        :search-label="`Search ${sectionTitle.toLowerCase()}`"
        :search-placeholder="`Search ${sectionTitle.toLowerCase()}…`">
        <template #actions>
            <Button v-if="section === 'mine'" size="sm" variant="primary" @click="emit('go', 'create')">+ New world</Button>
            <Button v-if="section === 'servers'" size="sm" @click="refreshServers()">Refresh</Button>
            <Button size="sm" @click="emit('go', 'home')">Back</Button>
        </template>

        <template #list>
            <div v-if="section === 'mine' && worlds === null" class="list-message">Reading your worlds…</div>
            <div v-else-if="section === 'friends' && store.account?.status === 'signedIn' && !store.social?.view" class="list-message">Finding friends' worlds…</div>
            <div v-else-if="section === 'servers' && directory === null" class="list-message">Finding public servers…</div>
            <template v-else-if="groups.length">
                <section v-for="group in groups" :key="group.label" class="list-group">
                    <h3>{{ group.label }}</h3>
                    <button v-for="entry in group.entries" :key="entry.id" class="entry" :class="{ on: selected === entry.id }"
                        :aria-current="selected === entry.id ? 'page' : undefined" @click="selected = entry.id">
                        <span class="entry-copy"><strong>{{ entry.label }}</strong><small>{{ entry.note }}</small></span>
                        <span v-if="entry.tag" class="entry-tag">{{ entry.tag }}</span>
                        <span v-if="selected === entry.id" class="entry-current" aria-hidden="true">»</span>
                    </button>
                </section>
            </template>
            <div v-else class="list-message">{{ query ? "No matches." : section === "mine" ? "No worlds saved yet." : section === "friends" ? "No worlds to join right now." : "No servers listed." }}</div>
        </template>

        <template #content>
            <Notice v-if="problem" tone="danger">{{ problem }}</Notice>
            <template v-if="local">
                <header class="detail-head">
                    <span class="eyebrow">Your world</span>
                    <div class="title-row"><h2>{{ local.name }}</h2><Button variant="primary" :disabled="busy" @click="play(local)">Play »</Button></div>
                    <p>{{ local.lastOpened ? `Last opened ${date(local.lastOpened)}` : "Ready for its first day" }}</p>
                </header>
                <div class="world-overview">
                    <div v-if="!local.code" class="no-map">No Terra planet code is saved for this world.</div>
                    <div class="world-facts">
                        <h3>World at a glance</h3>
                        <dl>
                            <div v-if="local.mode"><dt>Mode</dt><dd>{{ local.mode }}</dd></div>
                            <div v-if="local.rules.tf_difficulty"><dt>Difficulty</dt><dd>{{ local.rules.tf_difficulty }}</dd></div>
                            <div v-if="size"><dt>Planet</dt><dd>{{ Math.round(size / (2 * Math.PI) / 1000) }}k block radius</dd></div>
                            <div v-if="local.rules.tf_seasons"><dt>Seasons</dt><dd>{{ local.rules.tf_seasons === "true" ? "On" : "Off" }}</dd></div>
                            <div v-if="local.rules.tf_weather"><dt>Weather</dt><dd>{{ local.rules.tf_weather }}</dd></div>
                            <div v-if="installedMods"><dt>Added mods</dt><dd>{{ installedMods.length }}</dd></div>
                        </dl>
                        <Button size="sm" :disabled="busy" @click="pin(local)">{{ local.pinned ? "Unpin world" : "Pin world" }}</Button>
                    </div>
                </div>
                <section v-if="local.code" class="detail-section">
                    <h3>Planet code</h3>
                    <p>The same land and climate can be shared. Buildings and world rules stay in this save.</p>
                    <div class="section-actions">
                        <code class="world-code">{{ local.code }}</code>
                        <Button size="sm" @click="copier.copy(local.code!)">{{ copier.state.value === "copied" ? "Copied" : "Copy code" }}</Button>
                    </div>
                    <Notice v-if="copier.state.value === 'failed'" tone="warn">Clipboard access failed. Select the code to copy it.</Notice>
                </section>
                <section v-if="installedMods" class="detail-section">
                    <h3>Mods in this world</h3>
                    <p>{{ installedMods.length ? installedMods.map((mod) => mod.title).join(" · ") : "Base game only." }}</p>
                    <Button size="sm" @click="emit('go', 'community')">Browse mods »</Button>
                </section>
                <section class="danger-zone">
                    <div class="danger-copy"><h3>World management</h3><p>Remove this world and everything built in it from this device.</p></div>
                    <template v-if="confirming">
                        <span>Delete “{{ local.name }}” and its saved world?</span>
                        <Button size="sm" @click="confirming = false">Keep world</Button>
                        <Button size="sm" variant="danger" :disabled="busy" @click="remove(local)">Delete</Button>
                    </template>
                    <Button v-else size="sm" @click="confirming = true">Delete world</Button>
                </section>
            </template>

            <template v-else-if="friend">
                <header class="detail-head">
                    <span class="eyebrow">{{ friend.invite ? "Invitation" : "Friend's world" }}</span>
                    <div class="title-row"><h2>{{ friend.world }}</h2><Button variant="primary" :disabled="busy" @click="join(friend.player)">Join »</Button></div>
                    <p>{{ friend.invite ? "You've been invited to play." : "Open to friends now." }}</p>
                </header>
                <div class="host">
                    <span class="avatar">{{ friend.player.username.slice(0, 1).toUpperCase() }}</span>
                    <span><strong>{{ friend.player.name || friend.player.username }}</strong><small>@{{ friend.player.username }} · host</small></span>
                    <span class="online-dot" />
                </div>
                <section class="detail-section">
                    <h3>Joining this world</h3>
                    <p>Terra Firma will connect you to {{ friend.player.username }}'s session. Your own worlds stay on this device.</p>
                    <p v-if="friend.invite" class="dim">Invite expires {{ new Date(friend.invite.expiresAt).toLocaleString() }}.</p>
                </section>
                <Notice v-if="store.social?.problem" tone="warn">Friend status may be out of date: {{ store.social.problem }}</Notice>
            </template>

            <template v-else-if="server">
                <header class="detail-head">
                    <span class="eyebrow">Public server</span>
                    <div class="title-row"><h2>{{ server.name }}</h2><Button variant="primary" :disabled="busy || !server.compatible" @click="joinServer(server)">Join »</Button></div>
                    <p>Hosted by {{ server.host }}</p>
                </header>
                <p class="description">{{ server.description }}</p>
                <div class="server-stats">
                    <div><span>Players</span><strong>{{ server.players }} / {{ server.capacity }}</strong></div>
                    <div><span>Region</span><strong>{{ server.region }}</strong></div>
                    <div><span>Latency</span><strong>{{ server.pingMs === null ? "—" : `${server.pingMs} ms` }}</strong></div>
                    <div><span>Build</span><strong>{{ server.version }}</strong></div>
                </div>
                <section class="detail-section">
                    <h3>What to expect</h3>
                    <div class="tags"><span v-for="tag in server.tags" :key="tag">{{ tag }}</span></div>
                </section>
                <section class="detail-section">
                    <h3>Required mods</h3>
                    <p>{{ server.mods.length ? server.mods.join(" · ") : "Base game only." }}</p>
                </section>
                <Notice v-if="!server.compatible" tone="warn">This server uses a different game build. Update Terra Firma before joining.</Notice>
                <Button size="sm" :disabled="busy" @click="favorite(server)">{{ server.favorite ? "Remove favorite" : "Add to favorites" }}</Button>
            </template>

            <div v-else class="empty-detail">
                <span class="large-mark">»</span>
                <template v-if="section === 'mine'">
                    <h2>{{ query ? "No matching worlds" : "Your worlds" }}</h2>
                    <p>{{ query ? "Try another search." : "Create a planet, then return here to play it." }}</p>
                    <Button v-if="!query" variant="primary" @click="emit('go', 'create')">Create a world »</Button>
                </template>
                <template v-else-if="section === 'friends'">
                    <h2>Friends' worlds</h2>
                    <p v-if="store.account?.status === 'signedOut'">Connect your ArcLabs account to see invitations and worlds your friends have opened.</p>
                    <p v-else-if="store.account?.status === 'signedIn' && !store.social?.view">Checking who is playing…</p>
                    <p v-else-if="store.account?.status === 'signedIn'">When a friend opens a world or invites you, it appears here.</p>
                    <p v-else>Checking your account…</p>
                    <Button v-if="store.account?.status === 'signedOut'" variant="primary" :disabled="busy" @click="signIn()">Connect ArcLabs »</Button>
                    <Notice v-if="store.social?.problem" tone="warn">{{ store.social.problem }}</Notice>
                </template>
                <template v-else>
                    <h2>Public servers</h2>
                    <p v-if="directory?.status === 'unavailable'">The public server directory is being connected. Favorites and discovery will appear here.</p>
                    <p v-else-if="directory === null">Finding public worlds…</p>
                    <p v-else>{{ query ? "No servers match your search." : "No public servers are listed yet." }}</p>
                </template>
            </div>
        </template>
    </Dock>
</template>

<style scoped>
.list-group { margin-bottom: 14px; }
.list-group h3 { margin: 0 0 5px; padding: 0 8px; color: var(--ink-faint); font: 10px var(--font-display); text-transform: uppercase; letter-spacing: .07em; }
.entry { display: grid; grid-template-columns: minmax(0, 1fr) auto auto; align-items: center; gap: 7px; width: 100%; min-width: 0; padding: 8px 8px 8px 11px; border: 0; border-left: 3px solid transparent; background: none; color: var(--ink); text-align: left; cursor: pointer; }
.entry:hover { background: var(--stone-2); }
.entry.on { border-left-color: var(--accent); background: var(--stone-2); }
.entry-copy { min-width: 0; flex: 1; display: flex; flex-direction: column; }
.entry-copy strong, .entry-copy small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.entry-copy strong { font-size: 12px; font-weight: 400; }
.entry-copy small { color: var(--ink-faint); font-size: 10px; }
.entry-tag { flex: none; color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
.entry-current { color: var(--accent-hi); font: 15px var(--font-display); }
.list-message { padding: 8px; color: var(--ink-dim); font-size: 11px; }
.detail-head { display: grid; gap: 8px; padding-bottom: 18px; border-bottom: 1px solid var(--stone-4); }
.eyebrow { color: var(--accent-hi); font: 10px var(--font-display); text-transform: uppercase; }
.title-row { display: flex; align-items: center; justify-content: space-between; gap: 18px; }
h2 { margin: 0; font: 24px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.detail-head p, .description { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.55; }
.world-overview { display: grid; gap: 20px; padding: 8px 0; }
.world-overview :deep(.preview) { width: 100%; }
.no-map { display: grid; place-items: center; aspect-ratio: 1; border: var(--b) solid var(--edge); background: var(--stone-1); color: var(--ink-dim); font-size: 12px; text-align: center; padding: 24px; }
.world-facts { display: flex; flex-direction: column; align-items: flex-start; gap: 14px; min-width: 0; }
h3 { margin: 0; font: 12px var(--font-display); text-transform: uppercase; }
dl { display: grid; gap: 7px; width: 100%; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 14px; padding-bottom: 6px; border-bottom: 1px solid var(--stone-3); font-size: 11px; }
dt { color: var(--ink-dim); }
dd { margin: 0; text-align: right; font-family: var(--font-display); text-transform: uppercase; }
.detail-section { display: grid; justify-items: start; gap: 10px; padding-top: 18px; border-top: 1px solid var(--stone-4); }
.detail-section p { max-width: 65ch; margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
.section-actions { display: flex; align-items: stretch; gap: 8px; max-width: 100%; }
.world-code { max-width: 100%; overflow-wrap: anywhere; padding: 10px 12px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk); color: var(--accent-hi); font-size: 11px; user-select: text; }
.danger-zone { display: flex; align-items: end; justify-content: space-between; gap: 12px; flex-wrap: wrap; margin-top: 10px; padding: 18px 0 30px; border-top: 1px solid var(--stone-4); color: var(--danger); font-size: 11px; }
.danger-copy { display: grid; gap: 6px; }
.danger-copy p { max-width: 52ch; margin: 0; color: var(--ink-faint); font-size: 11px; line-height: 1.45; }
.host { display: flex; align-items: center; gap: 12px; padding: 14px; border: 1px solid var(--stone-4); background: var(--stone-1); }
.avatar { display: grid; place-items: center; width: 42px; height: 42px; border: var(--b) solid var(--edge); background: var(--accent-lo); box-shadow: var(--raised); font: 18px var(--font-display); }
.host > span:nth-child(2) { display: flex; flex-direction: column; flex: 1; min-width: 0; }
.host strong { font-size: 13px; }
.host small { color: var(--ink-dim); font-size: 11px; }
.online-dot { width: 9px; height: 9px; background: #70ba80; }
.server-stats { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 8px; }
.server-stats div { display: grid; gap: 5px; padding: 12px; border: 1px solid var(--stone-4); background: var(--stone-1); }
.server-stats span { color: var(--ink-faint); font: 9px var(--font-display); text-transform: uppercase; }
.server-stats strong { font: 13px var(--font-display); }
.tags { display: flex; flex-wrap: wrap; gap: 6px; }
.tags span { padding: 4px 8px; border: 1px solid var(--stone-4); background: var(--stone-1); color: var(--ink-dim); font-size: 11px; }
.play-panel :deep(.workbench-content) > .btn { align-self: flex-start; }
.empty-detail { display: grid; justify-items: start; align-content: center; gap: 12px; min-height: 100%; max-width: 440px; }
.empty-detail p { margin: 0; color: var(--ink-dim); font-size: 12px; line-height: 1.6; }
.large-mark { color: var(--accent); font: 36px var(--font-display); }
.play-panel :deep(.workbench-content-inner) { padding-bottom: 24px; }
@media (max-width: 900px) {
    .world-overview { grid-template-columns: 1fr; }
    .server-stats { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}
</style>
