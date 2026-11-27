<script setup lang="ts">
import { computed, ref } from "vue"
import type { Friend, Player } from "../bridge/index.ts"
import { useStore } from "../game/store.ts"
import { Card, CardSlot, ICONS, ListEmpty, Notice, ResultsHead, SideCard, SideNav, SideStats } from "../kit/index.ts"
import { byPresence, presenceLine } from "../social/presence.ts"
import type { ActionsHandle } from "./actions.ts"
import { places } from "./places.ts"

/**
 * Your friends, laid out as the library's Browse: a side card that stays put (which list, the
 * numbers, adding someone by name, your own page) beside a grid of cards, one a player. A card
 * opens their page, where you join, invite or answer them. The query comes from the screen's search.
 */
type List = "all" | "playing" | "online" | "offline" | "requests"
/** A card: a friend, or someone waiting on you or you on them. */
type Entry = { player: Player; line: string; badges: string[]; friend: Friend | null }
const props = defineProps<{ actions: ActionsHandle; query: string }>()
const emit = defineEmits<{ player: [handle: string] }>()
const store = useStore()
const list = ref<List>("all")
const typed = ref("")
const view = computed(() => store.social?.view ?? null)
const friends = computed(() => [...(view.value?.friends ?? [])].sort(byPresence))
const HOLDS: Record<Exclude<List, "requests">, (f: Friend) => boolean> = {
    all: () => true,
    playing: (f) => f.presence !== null && f.presence.state !== "menu",
    online: (f) => f.presence?.state === "menu",
    offline: (f) => f.presence === null,
}
const requests = computed<Entry[]>(() => [
    ...(view.value?.invites ?? []).map((i) => ({ player: i.from, line: `Invited you to ${i.world}`, badges: ["Invite"], friend: null })),
    ...(view.value?.incoming ?? []).map((p) => ({ player: p, line: "Wants to be friends", badges: ["Request"], friend: null })),
    ...(view.value?.outgoing ?? []).map((p) => ({ player: p, line: "Waiting for them", badges: ["Sent"], friend: null })),
])
const entries = computed<Entry[]>(() => {
    const chosen = list.value === "requests" ? requests.value
        : friends.value.filter(HOLDS[list.value]).map((f) => ({ player: f, line: presenceLine(f.presence), badges: places.joinable(f) ? ["Open"] : [], friend: f }))
    const q = props.query.trim().toLowerCase()
    return chosen.filter((e) => (e.player.username + " " + (e.player.name ?? "")).toLowerCase().includes(q))
})
const count = (list: Exclude<List, "requests">) => friends.value.filter(HOLDS[list]).length
const lists = computed(() => [
    { id: "all" as const, label: "All friends", icon: ICONS.players, count: friends.value.length },
    { id: "playing" as const, label: "Playing", icon: ICONS.play, count: count("playing") },
    { id: "online" as const, label: "Online", icon: ICONS.region, count: count("online") },
    { id: "offline" as const, label: "Offline", icon: ICONS.profile, count: count("offline") },
    { id: "requests" as const, label: "Requests", icon: ICONS.heart, count: requests.value.length },
])
const stats = computed(() => [
    { icon: ICONS.players, value: String(friends.value.length), label: "friends" },
    { icon: ICONS.play, value: String(count("playing")), label: "playing" },
    { icon: ICONS.heart, value: String((view.value?.incoming.length ?? 0) + (view.value?.invites.length ?? 0)), label: "waiting" },
])
const TITLES: Record<List, string> = { all: "Friends", playing: "Playing", online: "Online", offline: "Offline", requests: "Requests" }
const me = computed(() => store.account?.status === "signedIn" ? store.account.username : null)

async function add() {
    const name = typed.value.trim().replace(/^@/, "")
    if (!name || props.actions.busy.value) return
    if (await props.actions.request(name)) typed.value = ""
}
</script>

<template>
    <div class="friends-page">
        <SideCard class="side">
            <SideNav v-model="list" :items="lists" />
            <SideStats :items="stats" />
            <form class="add" @submit.prevent="add">
                <label for="add-friend">Add a friend</label>
                <div class="field">
                    <span class="plus">+</span>
                    <input id="add-friend" v-model="typed" type="text" placeholder="Their username" spellcheck="false" autocomplete="off" maxlength="64">
                    <button class="send" :disabled="actions.busy.value || !typed.trim()">Send</button>
                </div>
            </form>
            <button v-if="me" type="button" class="you" @click="emit('player', me)">
                <span>Your page</span><b>@{{ me }} »</b>
            </button>
        </SideCard>

        <div class="results">
            <ResultsHead :title="query.trim() ? `“${query.trim()}”` : TITLES[list]" :count="view ? `${entries.length} shown` : null" />
            <Notice v-if="actions.problem.value" tone="danger">{{ actions.problem.value }}</Notice>
            <Notice v-if="store.social?.problem" tone="warn">Friends may be out of date: {{ store.social.problem }}</Notice>
            <div v-if="!view" class="grid"><CardSlot v-for="n in 6" :key="n" /></div>
            <ListEmpty v-else-if="!entries.length" :title="query.trim() ? 'Nobody matches' : list === 'requests' ? 'Nothing waiting' : 'No one here'">
                <p>{{ list === "requests" ? "Friend requests and invites show up here." : friends.length ? "Try another list." : "Add someone by their username, beside the list." }}</p>
            </ListEmpty>
            <div v-else class="grid">
                <Card v-for="e in entries" :key="e.player.id" :title="e.player.name || `@${e.player.username}`" :seed="`@${e.player.username}`"
                    :by="e.player.name ? `@${e.player.username}` : null" :description="e.line" :badges="e.badges" @open="emit('player', e.player.username)">
                    <template #meta>
                        <span class="dot" :class="{ on: e.friend?.presence, open: e.friend && places.joinable(e.friend) }" />
                        {{ e.friend ? (e.friend.presence ? "Online" : "Offline") : e.badges[0] }}
                    </template>
                </Card>
            </div>
        </div>
    </div>
</template>

<style scoped>
.friends-page { display: grid; grid-template-columns: 17rem minmax(0, 1fr); gap: 3rem; align-items: start; }
.side { position: sticky; top: 2.5rem; }
.results { display: grid; gap: 1rem; min-width: 0; }
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(min(100%, 13rem), 1fr)); gap: 1rem; }
.add { display: grid; grid-template-columns: minmax(0, 1fr); gap: 0.5rem; }
label { color: var(--ink-faint); font: 0.58rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; }
.field { display: flex; align-items: center; gap: 0.5rem; height: 2.3rem; padding: 0 0.3rem 0 0.6rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); }
.field:focus-within { border-color: var(--edge-lit); }
.plus { color: var(--accent-hi); font: 0.8rem var(--font-display); }
input { flex: 1; min-width: 0; height: 100%; border: 0; outline: 0; background: none; color: var(--ink); font: 0.8rem var(--font); }
input::placeholder { color: var(--ink-faint); }
.send { flex: none; height: 1.6rem; padding: 0 0.55rem; border: 1px solid var(--edge-lit-hi); border-radius: var(--radius); background: var(--glass-lit); color: #fff; font: 0.55rem var(--font-display); text-transform: uppercase; }
.send:disabled { opacity: 0.45; }
.you { display: flex; justify-content: space-between; align-items: center; gap: 0.75rem; width: 100%; border: 0; background: none; color: var(--ink-faint); font: 0.62rem var(--font-display); text-transform: uppercase; text-align: left; }
.you b { overflow: hidden; color: var(--accent-hi); font: 0.75rem var(--font); text-overflow: ellipsis; text-transform: none; white-space: nowrap; }
.you:hover b { color: var(--ink); }
.dot { display: inline-block; width: 0.45rem; height: 0.45rem; margin-right: 0.5rem; background: var(--night-4); }
.dot.on { background: var(--alive); }
.dot.open { background: var(--accent-hi); }
@media (max-width: 900px) { .friends-page { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
