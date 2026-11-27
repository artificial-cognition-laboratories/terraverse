<script setup lang="ts">
import { computed, onActivated } from "vue"
import type { Friend, LocalServer, PublicServer } from "../bridge/index.ts"
import { useCache } from "../cache/index.ts"
import { Card, CardSlot, KeptNote, Notice, Shelf } from "../kit/index.ts"
import { useStore } from "../game/store.ts"
import type { ActionsHandle } from "./actions.ts"
import FriendWorld from "./FriendWorld.vue"
import LiveServers from "./LiveServers.vue"
import { places } from "./places.ts"

/**
 * The online screen's front page, laid out as the library's: a friend's open world leading, then
 * shelves of where friends are, the servers on this machine and the busiest public ones. What
 * isn't kept yet (recent servers) says so in its place.
 */
const props = defineProps<{ actions: ActionsHandle }>()
const emit = defineEmits<{ player: [handle: string]; server: [server: PublicServer]; browse: []; friends: [] }>()
const store = useStore()
const directory = useCache().cached("servers.list")
const WIDTH = 4
const playing = computed(() => places.playing(store.social))
const lead = computed<Friend | null>(() => playing.value.find(places.joinable) ?? null)
const inMenu = computed(() => store.social?.me.state === "menu")
const local = computed(() => directory.data.value?.local ?? [])
const popular = computed(() => directory.data.value?.status === "ready" ? places.popular(directory.data.value.servers, 5) : [])
const popularEmpty = computed(() => directory.problem.value && !directory.data.value ? `The server list can't be reached: ${directory.problem.value}`
    : !directory.data.value ? "Asking for the server list…"
    : directory.data.value.status === "unavailable" ? "Public servers aren't listed yet: the directory isn't connected." : "No public servers yet.")
const signedIn = computed(() => store.account?.status === "signedIn")
const joinLocal = (s: LocalServer) => props.actions.joinServer({ id: s.ref, ref: s.ref })
onActivated(() => { void directory.run({}) })
</script>

<template>
    <div class="discover">
        <Notice v-if="actions.problem.value" tone="danger">{{ actions.problem.value }}</Notice>
        <KeptNote v-if="directory.problem.value && directory.at.value !== null" :at="directory.at.value" :problem="directory.problem.value" />

        <section v-if="lead?.presence" class="lead">
            <p class="kicker">» A friend's world is open</p>
            <h2>{{ lead.presence.world }}</h2>
            <p class="line"><button type="button" class="who" @click="emit('player', lead.username)">{{ lead.name || lead.username }}</button> is hosting, and friends can join.</p>
            <button type="button" class="go" :disabled="actions.busy.value || !inMenu" :title="inMenu ? undefined : 'Leave your world first'"
                @click="actions.joinFriend(lead)">{{ actions.joining.value === lead.id ? "Joining…" : "Join »" }}</button>
        </section>
        <section v-else class="lead quiet">
            <p class="kicker">» Online</p>
            <h2>Play together</h2>
            <p class="line">{{ signedIn ? "When a friend opens their world, it shows up here to join." : "Sign in to see friends and join their worlds. Servers on this machine work signed out." }}</p>
        </section>

        <Shelf title="Friends playing" :more="signedIn ? 'All friends' : null" @more="emit('friends')">
            <FriendWorld v-for="f in playing.slice(0, WIDTH)" :key="f.id" :friend="f" :busy="actions.busy.value" :in-menu="inMenu"
                :joining="actions.joining.value === f.id" @open="emit('player', f.username)" @join="actions.joinFriend(f)" />
            <CardSlot v-for="n in Math.max(0, WIDTH - playing.length)" :key="'f' + n" />
        </Shelf>
        <p v-if="!playing.length" class="under">{{ signedIn ? "No friends in a world right now." : "Signed out: no friends to show." }}</p>

        <Shelf title="On this machine" more="All servers" @more="emit('browse')">
            <Card v-for="s in local.slice(0, WIDTH)" :key="s.ref" :title="s.title" :seed="s.folder" :by="s.owner ? `run by @${s.owner}` : 'run signed out'"
                :description="s.ref" dense @open="joinLocal(s)">
                <template #meta><span class="dot" />Port {{ s.port }} · {{ s.mods.length }} mods · press to join</template>
            </Card>
            <CardSlot v-for="n in Math.max(0, WIDTH - local.length)" :key="'l' + n" />
        </Shelf>
        <p v-if="!local.length" class="under">Servers you run with <code>terra serve</code> or <code>terra dev</code> show up here.</p>

        <Shelf title="Recent servers" :cols="null">
            <!-- unwired: nothing records where you've played yet -->
            <Notice tone="info">Recent servers aren't kept yet: where you've played will show here.</Notice>
        </Shelf>

        <Shelf title="Popular servers" more="Browse servers" :cols="null" @more="emit('browse')">
            <LiveServers :servers="popular" :empty="popularEmpty" @open="(s) => emit('server', s)" />
        </Shelf>
    </div>
</template>

<style scoped>
/* the library's banner, as words over the sky: what to do first */
.lead { position: relative; display: grid; gap: 0.6rem; justify-items: start; padding: 2.25rem 2.5rem; border: 1px solid var(--edge-lit); border-radius: var(--radius);
    background: linear-gradient(100deg, rgb(10 75 145 / 0.35), rgb(7 11 17 / 0.6) 70%); backdrop-filter: blur(8px); }
.lead.quiet { border-color: var(--glass-edge); background: var(--glass); }
.kicker { margin: 0; color: var(--accent-hi); font: 0.6rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
h2 { margin: 0; color: var(--ink); font: 400 clamp(1.4rem, 3vw, 2rem) var(--font-display); text-transform: uppercase; }
.line { margin: 0; color: var(--ink-dim); font: 0.95rem/1.5 var(--font-sans); }
.who { padding: 0; border: 0; background: none; color: var(--accent-hi); font: inherit; }
.who:hover { color: var(--ink); }
.go { margin-top: 0.6rem; height: 2.4rem; padding: 0 1.4rem; border: 1px solid var(--edge-lit-hi); border-radius: var(--radius); background: var(--glass-lit); color: #fff;
    font: 0.66rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
.go:disabled { opacity: 0.5; }
.under { margin: 0.75rem 0 0; color: var(--ink-faint); font-size: 0.72rem; }
.dot { display: inline-block; width: 0.45rem; height: 0.45rem; margin-right: 0.5rem; background: var(--alive); }
</style>
