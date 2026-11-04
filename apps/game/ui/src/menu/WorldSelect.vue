<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { BridgeError, type CommunityCatalog, type Friend, type Invite, type Player, type World } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import PixelIcon from "../game/PixelIcon.vue"
import { useStore } from "../game/store.ts"
import { Button, Dock, Field, Notice } from "../kit/index.ts"
import { usePlanetarium } from "../planetarium/use.ts"
import { decodeWorldCode } from "../worldgen/config.ts"
import { Copier } from "./create/copy.ts"
import type { Screen } from "./screens.ts"
import { chipsOf, GEAR, PIN, planetIcon, RAIL, sizeLabel, type Section } from "./worldIcons.ts"

type Entry =
    | { kind: "local"; id: string; label: string; search: string; world: World }
    | { kind: "friend"; id: string; label: string; note: string; tag: string; search: string; player: Player; world: string; invite?: Invite }
type ListGroup = { label: string; entries: Entry[] }

const emit = defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()
const store = useStore()
const copier = Copier()
const section = ref<Section>("mine")
const query = ref("")
const worlds = ref<World[] | null>(null)
const catalog = ref<CommunityCatalog | null>(null)
const selected = ref<string | null>(null)
const details = ref(false) // the selected world's own page, in the panel
const confirming = ref(false)
const busy = ref(false)
const problem = ref<string | null>(null)
const naming = ref("")

const orderedWorlds = computed(() => [...(worlds.value ?? [])].sort((a, b) =>
    Number(b.pinned) - Number(a.pinned) || (b.lastOpened ?? 0) - (a.lastOpened ?? 0) || a.name.localeCompare(b.name)))
const localEntry = (world: World): Entry => ({
    kind: "local", id: world.path, label: world.name,
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
const groups = computed<ListGroup[]>(() => {
    const raw: ListGroup[] = section.value === "mine"
        ? [
            { label: "Pinned", entries: orderedWorlds.value.filter((w) => w.pinned).map(localEntry) },
            { label: "Recent", entries: orderedWorlds.value.filter((w) => !w.pinned && w.lastOpened !== null).map(localEntry) },
            { label: "New worlds", entries: orderedWorlds.value.filter((w) => !w.pinned && w.lastOpened === null).map(localEntry) },
        ]
        : friendGroups.value
    const term = query.value.trim().toLowerCase()
    return raw.map((group) => ({ ...group, entries: group.entries.filter((entry) => entry.search.toLowerCase().includes(term)) }))
        .filter((group) => group.entries.length)
})
const entries = computed(() => groups.value.flatMap((group) => group.entries))
const current = computed(() => entries.value.find((entry) => entry.id === selected.value) ?? null)
const local = computed(() => current.value?.kind === "local" ? current.value.world : null)
const friend = computed(() => current.value?.kind === "friend" ? current.value : null)

// the world selected is the universe behind: its system, the camera flown to
// its lit side, framed in what the panel leaves (src/shell/planetarium.h)
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
    if (now) planet.fly({ radii: 1.5, lit: true, seconds: 2 })
})
const installedMods = computed(() => catalog.value?.mods.filter((mod) =>
    mod.source === "installed" && local.value && mod.enabledWorlds.includes(local.value.path)) ?? null)
const modsOf = (world: World) => catalog.value?.mods.filter((mod) => mod.source === "installed" && mod.enabledWorlds.includes(world.path)).length ?? 0
function planetSize(world: World) {
    if (!world.code) return null
    try { return sizeLabel(decodeWorldCode(world.code).size) }
    catch { return null } // an older build's code: nothing to say about it
}
watch(section, () => { query.value = ""; selected.value = null; details.value = false; confirming.value = false })
watch(entries, (items) => {
    if (!items.some((item) => item.id === selected.value)) selected.value = items[0]?.id ?? null
}, { immediate: true })
watch(selected, () => { confirming.value = false; problem.value = null })
watch(details, (open) => { if (open && local.value) naming.value = local.value.name; confirming.value = false })

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
function signIn() { return action(() => bridge.send("account.signIn", {})) }
function pin(world: World) { return action(async () => {
    await bridge.send("worlds.pin", { path: world.path, pinned: !world.pinned })
    await refreshWorlds()
}) }
function rename(world: World) { return action(async () => {
    await bridge.send("worlds.rename", { path: world.path, name: naming.value })
    await refreshWorlds()
}) }
function remove(world: World) { return action(async () => {
    await bridge.send("worlds.delete", { path: world.path })
    confirming.value = false
    details.value = false
    await refreshWorlds()
}) }
// a double click is the short way: most visits are pick a world and go
function activate(entry: Entry) {
    if (entry.kind === "local") void play(entry.world)
    else void join(entry.player)
}
const orbits = (shown: boolean) => bridge.send("planetarium.orbits", { shown }).catch(report)
onMounted(() => {
    void orbits(false)
    void refreshWorlds()
    void bridge.send("community.list", {}).then((value) => { catalog.value = value }).catch(report)
})
onBeforeUnmount(() => { void orbits(true) })
</script>

<template>
    <div class="play-page">
    <Dock v-model:group="section" v-model:query="query" v-model:covers="covers"
        title="Play" section="" :groups="RAIL" rail-label="Places to play"
        :search-label="details ? undefined : 'Search worlds'" search-placeholder="Search…">
        <template #actions>
            <Button size="sm" @click="details ? details = false : emit('go', 'home')">Back</Button>
        </template>

        <template v-if="details && local" #workspace>
            <div class="details">
                <h2>{{ local.name }}</h2>
                <p class="dim">{{ local.lastOpened ? `Last opened ${date(local.lastOpened)}` : "Ready for its first day" }}</p>

                <section>
                    <h3>Name</h3>
                    <form class="row" @submit.prevent="rename(local)">
                        <Field v-model="naming" label="World name" />
                        <Button size="sm" type="submit" :disabled="busy || !naming.trim() || naming.trim() === local.name">Rename</Button>
                    </form>
                </section>

                <section>
                    <h3>This world</h3>
                    <dl>
                        <div v-if="local.mode"><dt>Mode</dt><dd>{{ local.mode }}</dd></div>
                        <div v-if="local.rules.tf_difficulty"><dt>Difficulty</dt><dd>{{ local.rules.tf_difficulty }}</dd></div>
                        <div v-if="planetSize(local)"><dt>Planet</dt><dd>{{ planetSize(local) }} block radius</dd></div>
                        <div v-if="local.rules.tf_seasons"><dt>Seasons</dt><dd>{{ local.rules.tf_seasons === "true" ? "On" : "Off" }}</dd></div>
                        <div v-if="local.rules.tf_weather"><dt>Weather</dt><dd>{{ local.rules.tf_weather }}</dd></div>
                    </dl>
                    <p class="dim">These were set when the world was made, and stay as they are.</p>
                    <Button size="sm" :disabled="busy" @click="pin(local)">{{ local.pinned ? "Unpin world" : "Pin world" }}</Button>
                </section>

                <section v-if="local.code">
                    <h3>Planet code</h3>
                    <p class="dim">The same land and climate can be shared. Buildings and world rules stay in this save.</p>
                    <code class="world-code">{{ local.code }}</code>
                    <Button size="sm" @click="copier.copy(local.code!)">{{ copier.state.value === "copied" ? "Copied" : "Copy code" }}</Button>
                    <Notice v-if="copier.state.value === 'failed'" tone="warn">Clipboard access failed. Select the code to copy it.</Notice>
                </section>

                <section v-if="installedMods">
                    <h3>Mods</h3>
                    <p class="dim">{{ installedMods.length ? installedMods.map((mod) => mod.title).join(" · ") : "Base game only." }}</p>
                    <Button size="sm" @click="emit('go', 'community')">Browse mods »</Button>
                </section>

                <section class="danger">
                    <h3>Delete</h3>
                    <p class="dim">Removes this world and everything built in it from this device.</p>
                    <template v-if="confirming">
                        <p>Delete “{{ local.name }}” and its saved world?</p>
                        <div class="row">
                            <Button size="sm" @click="confirming = false">Keep world</Button>
                            <Button size="sm" variant="danger" :disabled="busy" @click="remove(local)">Delete</Button>
                        </div>
                    </template>
                    <Button v-else size="sm" @click="confirming = true">Delete world…</Button>
                </section>
            </div>
        </template>

        <template v-else #list>
            <div v-if="section === 'mine' && worlds === null" class="list-message">Reading your worlds…</div>
            <div v-else-if="section === 'friends' && store.account?.status === 'signedIn' && !store.social?.view" class="list-message">Finding friends' worlds…</div>
            <template v-else-if="groups.length">
                <section v-for="group in groups" :key="group.label" class="list-group">
                    <h3>{{ group.label }}</h3>
                    <button v-for="entry in group.entries" :key="entry.id" class="entry" :class="{ on: selected === entry.id }"
                        :aria-current="selected === entry.id ? 'true' : undefined"
                        @click="selected = entry.id" @dblclick="activate(entry)">
                        <template v-if="entry.kind === 'local'">
                            <PixelIcon class="thumb" v-bind="planetIcon(entry.world.code ?? entry.world.path)" />
                            <span class="copy">
                                <span class="line"><strong>{{ entry.label }}</strong>
                                    <PixelIcon v-if="entry.world.pinned" class="pin" v-bind="PIN" aria-label="Pinned" /></span>
                                <span class="chips">
                                    <span v-for="chip in chipsOf(entry.world, modsOf(entry.world))" :key="chip.label" class="chip" :title="chip.label">
                                        <PixelIcon class="chip-icon" :rows="chip.rows" :palette="chip.palette" />{{ chip.text }}
                                    </span>
                                </span>
                                <small>{{ entry.world.lastOpened ? date(entry.world.lastOpened) : "Not played yet" }}<template v-if="planetSize(entry.world)"> · {{ planetSize(entry.world) }} radius</template></small>
                            </span>
                        </template>
                        <template v-else>
                            <span class="avatar">{{ entry.player.username.slice(0, 1).toUpperCase() }}</span>
                            <span class="copy"><strong>{{ entry.label }}</strong><small>{{ entry.note }}</small></span>
                            <span class="tag">{{ entry.tag }}</span>
                        </template>
                    </button>
                </section>
            </template>
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
    </Dock>

    <div class="go">
        <span v-if="problem" class="problem" role="alert">{{ problem }}</span>
        <div class="buttons">
            <Button v-if="section === 'mine'" @click="emit('go', 'create')">+ New world</Button>
            <template v-if="local && !details">
                <Button aria-label="World settings" title="World settings" @click="details = true"><PixelIcon class="gear" v-bind="GEAR" /></Button>
                <Button variant="primary" :disabled="busy" @click="play(local)">Play »</Button>
            </template>
            <Button v-else-if="local" variant="primary" :disabled="busy" @click="play(local)">Play »</Button>
            <Button v-else-if="friend" variant="primary" :disabled="busy" @click="join(friend.player)">Join »</Button>
        </div>
    </div>
    </div>
</template>

<style scoped>
.go { position: fixed; right: 16px; bottom: 16px; display: flex; flex-direction: column; align-items: end; gap: 6px; }
.buttons { display: flex; gap: 8px; }
.gear { width: 18px; height: 18px; }
.problem { max-width: 320px; padding: 6px 8px; border: var(--b) solid var(--edge); background: rgb(12 14 16 / 0.85); color: var(--danger); font-size: 11px; }
.list-group { margin: 8px 0 14px; }
.list-group h3 { margin: 0 0 5px; padding: 0 10px; color: var(--ink-faint); font: 10px var(--font-display); text-transform: uppercase; letter-spacing: .07em; }
.entry { display: grid; grid-template-columns: 40px minmax(0, 1fr) auto; align-items: center; gap: 10px; width: 100%; min-width: 0; padding: 9px 10px 9px 9px; border: 0; border-left: 3px solid transparent; background: none; color: var(--ink); text-align: left; cursor: pointer; }
.entry:hover { background: var(--stone-2); }
.entry.on { border-left-color: var(--accent); background: var(--stone-2); }
.thumb { width: 40px; height: 40px; }
.copy { display: flex; flex-direction: column; gap: 4px; min-width: 0; }
.line { display: flex; align-items: center; gap: 6px; min-width: 0; }
.copy strong { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 13px; font-weight: 400; }
.copy small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--ink-faint); font-size: 10px; }
.pin { flex: none; width: 11px; height: 11px; }
.chips { display: flex; flex-wrap: wrap; gap: 3px 9px; min-width: 0; }
.chip { display: inline-flex; align-items: center; gap: 4px; color: var(--ink-dim); font: 9px var(--font-display); text-transform: uppercase; white-space: nowrap; }
.chip-icon { width: 11px; height: 11px; }
.avatar { display: grid; place-items: center; width: 40px; height: 40px; border: var(--b) solid var(--edge); background: var(--accent-lo); box-shadow: var(--raised); font: 16px var(--font-display); }
.tag { color: var(--accent-hi); font: 9px var(--font-display); text-transform: uppercase; }
.list-message { display: grid; gap: 10px; justify-items: start; padding: 10px; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.list-message p { margin: 0; }
.details { flex: 1; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: 16px; padding: 16px 14px 24px; }
.details h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; overflow-wrap: anywhere; }
.details section { display: grid; justify-items: start; gap: 9px; padding-top: 14px; border-top: 1px solid var(--stone-4); }
.details h3 { margin: 0; font: 11px var(--font-display); text-transform: uppercase; }
.dim { margin: 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.row { display: flex; align-items: end; gap: 8px; width: 100%; min-width: 0; }
.row :deep(.field), .row > :first-child:not(.btn) { flex: 1; min-width: 0; }
dl { display: grid; gap: 6px; width: 100%; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 14px; padding-bottom: 5px; border-bottom: 1px solid var(--stone-3); font-size: 11px; }
dt { color: var(--ink-dim); }
dd { margin: 0; text-align: right; font-family: var(--font-display); text-transform: uppercase; font-size: 10px; }
.world-code { max-width: 100%; overflow-wrap: anywhere; padding: 8px 10px; border: var(--b) solid var(--edge); background: var(--stone-1); box-shadow: var(--sunk); color: var(--accent-hi); font-size: 11px; user-select: text; }
.danger h3 { color: var(--danger); }
.danger p:not(.dim) { margin: 0; color: var(--danger); font-size: 11px; }
</style>
