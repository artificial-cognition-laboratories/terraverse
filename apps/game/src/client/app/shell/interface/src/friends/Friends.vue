<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import { Copier, Cover, Monogram, Notice, onEscape } from "../kit/index.ts"
import { Actions } from "../online/actions.ts"
import AccountsView from "./AccountsView.vue"
import FriendRow from "./FriendRow.vue"
import { groups } from "./groups.ts"
import type { LibraryOpen } from "../menu/library/icons.ts"

/**
 * The friends popover: a floating list under the account corner, the same in the menu and in game.
 * You on top (click for your accounts), a search that filters your friends and, for a name that
 * isn't one, offers to send them a request; then requests, who's in a world, who's around and
 * who's away, each row with its one action. Always the same height. Signed out, it's the way in:
 * the accounts this machine remembers, and signing in another, in place.
 */
/** `player`: a friend's page asked for, which the place showing the popover opens in the library. */
const emit = defineEmits<{ close: []; library: [to: LibraryOpen] }>()
const store = useStore()
const actions = Actions({ bridge: useBridge() })
const copier = Copier()
const mode = ref<"list" | "accounts">("list")
// switching or adding an account passes through signed out: the last one in holds the popover
// still meanwhile, so it never flashes to the way in
const switching = ref<string | null>(null)
const adding = ref(false)
let cancelled = false
const last = ref<{ username: string; name: string | null } | null>(null)
const me = computed(() => {
    const account = store.account
    if (account?.status === "signedIn") return account
    return switching.value || adding.value ? last.value : null
})
watch(() => store.account, (account) => {
    if (account?.status !== "signedIn") return
    last.value = { username: account.username, name: account.name }
    switching.value = null
    adding.value = false
    // in by a sign-in or a switch: to your friends; back by cancelling one: to the accounts, where it began
    mode.value = cancelled ? "accounts" : "list"
    cancelled = false
}, { immediate: true })
async function switchTo(name: string) {
    switching.value = name
    if (!(await actions.send("profiles.use", { name: name }))) switching.value = null
}
// signed in, adding leaves this account (remembered) for a new sign-in; cancelling comes back to it
async function addAccount() {
    if (me.value) {
        adding.value = true
        if (!(await actions.send("account.leave", {}))) { adding.value = false; return }
    }
    if (!(await actions.send("account.signIn", {}))) cancelAdd()
}
// a sign-in under way: closing the popover, Esc or your row in its head each cancel it, one way out
const signingIn = computed(() => {
    const status = store.account?.status
    return status === "starting" || status === "pending" || status === "checking" || status === "unreachable"
})
function headClicked() {
    if (signingIn.value) void cancelAdd()
    else mode.value = mode.value === "accounts" ? "list" : "accounts"
}
onBeforeUnmount(() => { if (signingIn.value) void cancelAdd() })
async function cancelAdd() {
    if (store.account?.status !== "signedOut") await actions.send("account.cancel", {})
    if (adding.value && last.value) {
        cancelled = true
        await switchTo(`@${last.value.username}`)
    }
    adding.value = false
}
const view = computed(() => store.social?.view ?? null)
const query = ref("")
const name = computed(() => query.value.trim().replace(/^@/, ""))
const list = computed(() => view.value ? groups(view.value, name.value) : [])
// a name that's none of your friends' (nor already asked) can be sent a request from the search
const known = computed(() => !!view.value && [...view.value.friends, ...view.value.incoming, ...view.value.outgoing]
    .some((p) => p.username.toLowerCase() === name.value.toLowerCase()))
const canInvite = computed(() => !!store.social?.joinable)
const offlineOpen = ref(true)
const sent = ref<string | null>(null)

async function add() {
    const who = name.value
    if (!who || known.value) return
    if (await actions.request(who)) {
        sent.value = who
        query.value = ""
    }
}
onEscape(() => {
    if (signingIn.value) void cancelAdd()
    else if (mode.value !== "list") mode.value = "list"
    else if (query.value) query.value = ""
    else emit("close")
})
</script>

<template>
    <div class="scrim" @mousedown.self="emit('close')">
        <section class="pop" role="dialog" aria-label="Friends">
            <template v-if="me">
                <header class="head">
                    <Cover class="banner" :seed="me.username" :width="48" />
                    <button class="me" :class="{ on: mode === 'accounts' }" @click="headClicked">
                        <span class="face"><Monogram :name="me.username" :size="38" /><span class="dot" /></span>
                        <span class="who"><span class="name">{{ me.name ?? me.username }}</span><span class="sub">@{{ me.username }} · online</span></span>
                        <span class="chev">{{ mode === "accounts" ? "▴" : "▾" }}</span>
                    </button>
                </header>
                <form v-if="mode === 'list'" class="search" @submit.prevent="add">
                    <svg viewBox="0 0 16 16" width="13" height="13" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.5" d="M7 2.5a4.5 4.5 0 1 1 0 9 4.5 4.5 0 0 1 0-9zM10.3 10.3 14 14" /></svg>
                    <input v-model="query" placeholder="Search friends, or add by username" spellcheck="false" autocomplete="off" @input="sent = null">
                </form>
                <Notice v-if="actions.problem.value" tone="danger" class="problem">{{ actions.problem.value }}</Notice>
                <AccountsView v-if="mode === 'accounts'" :actions="actions" :me="me" :switching="switching" @switch="switchTo" @add="addAccount" @cancel="cancelAdd" @manage="emit('library', { page: 'account' })" />
                <div v-else class="body">
                    <p v-if="sent" class="sent">Request sent to @{{ sent }}.</p>
                    <button v-if="name && !known" class="invite-new" :disabled="actions.busy.value" @click="add">
                        <span class="plus">+</span>
                        <span class="who"><span class="name">Add @{{ name }}</span><span class="sub">Send a friend request</span></span>
                        <span class="enter">↵</span>
                    </button>
                    <p v-if="!view" class="quiet">Finding your friends…</p>
                    <p v-else-if="!list.length && !name" class="quiet">No friends yet. Type a username above to add one.</p>
                    <section v-for="g in list" :key="g.id" class="group">
                        <button v-if="g.id === 'offline'" class="label toggle" @click="offlineOpen = !offlineOpen">
                            {{ g.label }} · {{ g.friends.length }} <span>{{ offlineOpen ? "▾" : "▸" }}</span>
                        </button>
                        <p v-else class="label">{{ g.label }} · {{ "players" in g ? g.players.length : g.friends.length }}</p>
                        <ul v-if="'players' in g" class="rows">
                            <li v-for="p in g.players" :key="p.id" class="request tile">
                                <Monogram :name="p.username" :size="30" />
                                <span class="who"><span class="name">{{ p.name ?? p.username }}</span><span class="sub">Wants to be friends</span></span>
                                <button class="icon ok" :disabled="actions.busy.value" aria-label="Accept" title="Accept" @click="actions.accept(p.id)">✓</button>
                                <button class="icon" :disabled="actions.busy.value" aria-label="Decline" title="Decline" @click="actions.remove(p.id)">✕</button>
                            </li>
                        </ul>
                        <ul v-else-if="g.id !== 'offline' || offlineOpen || name" class="rows">
                            <FriendRow v-for="f in g.friends" :key="f.id" :friend="f" :can-invite="canInvite"
                                :busy="actions.busy.value" :joining="actions.joining.value === f.id"
                                @join="actions.joinFriend(f)" @invite="actions.invite(f.id)" @remove="actions.remove(f.id)"
                                @copy="copier.copy(`@${f.username}`)" @page="emit('library', { player: f.username })" />
                        </ul>
                    </section>
                </div>
            </template>
            <template v-else>
                <header class="head">
                    <button class="me" :class="{ still: !signingIn }" :disabled="!signingIn" @click="headClicked">
                        <span class="face"><span class="nobody">?</span></span>
                        <span class="who"><span class="name">Not signed in</span><span class="sub">Friends need an ArcLabs account</span></span>
                    </button>
                </header>
                <Notice v-if="actions.problem.value" tone="danger" class="problem">{{ actions.problem.value }}</Notice>
                <AccountsView :actions="actions" :me="null" :switching="switching" @switch="switchTo" @add="addAccount" @cancel="cancelAdd" @manage="emit('library', { page: 'account' })" />
            </template>
        </section>
    </div>
</template>

<style scoped>
/* under the account corner (z 3), so its icon stays shown and toggles this shut */
.scrim { position: fixed; inset: 0; z-index: 2; }
/* as the inventory: the dark glass outside, its tiles a step lighter within */
.pop {
    position: absolute; top: 72px; right: 20px; display: flex; flex-direction: column;
    width: 320px; height: min(78vh, 620px); overflow: hidden;
    border: 1px solid var(--glass-edge-hi); border-radius: var(--radius);
    background: rgb(3 5 8 / 0.86); box-shadow: var(--sheen), 0 12px 40px rgb(0 0 0 / 0.45); backdrop-filter: blur(14px);
}
/* you, over your own pixel landscape, as your player page opens */
.head { position: relative; padding: 0.5rem; border-bottom: 1px solid var(--glass-edge); }
/* the landscape as tinted glass: faint, and fading into the pane towards your name */
.banner { position: absolute; inset: 0; opacity: 0.4; }
.banner::after { content: ""; position: absolute; inset: 0; background: linear-gradient(90deg, rgb(5 8 12 / 0.85) 25%, rgb(5 8 12 / 0.15)); backdrop-filter: blur(1px); }
.me { position: relative; display: flex; align-items: center; gap: 0.75rem; width: 100%; min-width: 0; padding: 0.5rem; border: 1px solid transparent; border-radius: var(--radius); background: none; text-align: left; }
.me:hover, .me.on { border-color: var(--glass-edge); background: rgb(4 7 11 / 0.4); }
.face { position: relative; display: block; flex: none; }
.dot { position: absolute; right: -3px; bottom: -3px; width: 9px; height: 9px; border: 1px solid var(--edge); border-radius: var(--radius); background: var(--accent-hi); }
.who { display: flex; flex: 1; flex-direction: column; gap: 0.3rem; min-width: 0; }
.name, .sub { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.name { color: var(--ink); font: 0.7rem var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; }
.sub { color: var(--ink-faint); font: 0.68rem var(--font); }
.chev { color: var(--ink-faint); font-size: 0.7rem; }
.search { position: relative; padding: 0.6rem; border-bottom: 1px solid var(--glass-edge); }
.search svg { position: absolute; top: 50%; left: 1.15rem; color: var(--ink-faint); transform: translateY(-50%); pointer-events: none; }
.search input { width: 100%; height: 30px; padding: 0 0.6rem 0 1.9rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: none; color: var(--ink); font: 0.75rem var(--font); outline: none; }
.search input::placeholder { color: var(--ink-faint); }
.search input:focus { border-color: var(--edge-lit-hi); }
.problem { margin: 0.6rem; }
.body, .pop > :deep(.accounts) { flex: 1; min-height: 0; }
.head, .search, .problem { flex: none; }
.body { display: flex; flex-direction: column; gap: 1rem; overflow-y: auto; padding: 0.85rem 0.6rem; }
.quiet { margin: 1rem 0.25rem; color: var(--ink-faint); font: 0.75rem/1.5 var(--font); text-align: center; }
.sent { margin: 0; color: var(--accent-hi); font: 0.72rem var(--font); }
.group { display: flex; flex-direction: column; gap: 0.35rem; }
/* a section's name over a hairline, as settings heads its sections */
.label { display: flex; gap: 0.35rem; align-items: center; width: 100%; margin: 0 0 0.2rem; padding: 0 0.1rem 0.4rem; border: 0; border-bottom: 1px solid var(--glass-edge); background: none; color: var(--ink-faint); font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; text-align: left; }
.toggle:hover { color: var(--ink-dim); }
.toggle span { margin-left: auto; }
.rows { display: flex; flex-direction: column; gap: 0.35rem; margin: 0; padding: 0; list-style: none; }
.tile, .invite-new { display: flex; align-items: center; gap: 0.75rem; min-width: 0; padding: 0.55rem 0.65rem; border: 1px solid rgb(231 237 244 / 0.05); border-radius: var(--radius); background: rgb(231 237 244 / 0.045); text-align: left; }
.invite-new { flex: none; border-style: dashed; border-color: rgb(99 163 233 / 0.22); }
.invite-new:hover:not(:disabled), .tile:hover { border-color: var(--glass-edge-hi); }
.plus { display: grid; place-items: center; flex: none; width: 30px; height: 30px; color: var(--accent-hi); font: 0.9rem var(--font-display); }
.enter { color: var(--ink-faint); font-size: 0.75rem; }
.icon { display: grid; place-items: center; flex: none; width: 26px; height: 26px; border: 1px solid var(--glass-edge-hi); border-radius: var(--radius); background: none; color: var(--ink-dim); font: 0.75rem var(--font); }
.icon:hover:not(:disabled) { border-color: var(--edge-lit-hi); color: var(--ink); }
.icon.ok { border-color: var(--accent); background: var(--accent-lo); color: var(--accent-hi); }
.me.still { cursor: default; }
.me.still:hover { border-color: transparent; background: none; }
.nobody { display: grid; place-items: center; width: 38px; height: 38px; border: 1px dashed var(--glass-edge-hi); border-radius: var(--radius); color: var(--ink-faint); font: 0.8rem var(--font-display); }
</style>
