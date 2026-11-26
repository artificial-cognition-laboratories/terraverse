<script setup lang="ts">
import { computed, ref } from "vue"
import type { Account, Player, RememberedProfile, Social } from "../../bridge"
import AccountPage from "./AccountPage.vue"
import Friends from "./Friends.vue"
import Inbox from "./Inbox.vue"
import SignIn from "./SignIn.vue"
import "./rows.css"

/**
 * The account and friends, under the corner's button: who you are, then Friends, Inbox and
 * Account as tabs; signed out, the way in. The inventory's glass. Every verb is the caller's
 * (account/Corner.vue): this only says what was asked.
 */
export type Tab = "friends" | "inbox" | "account"
const props = defineProps<{ account: Account; social: Social | null; error: string | null; busy: boolean; joining: string | null; remembered: RememberedProfile[] | null; initialTab?: Tab }>()
const emit = defineEmits<{
    signIn: []; cancel: []; signOut: []; leave: []; retry: []; openUrl: [url: string]
    use: [name: string]; forget: [name: string]
    request: [username: string]; accept: [id: string]; remove: [id: string]
    invite: [id: string]; dismiss: [id: string]; join: [player: Player]
}>()

const tab = ref<Tab>(props.initialTab ?? "friends")
const view = computed(() => props.social?.view ?? null)
const waiting = computed(() => (view.value?.incoming.length ?? 0) + (view.value?.invites.length ?? 0))
// the rail's icons (16px, drawn in the pixel grid); Account sits at its foot
const TABS: readonly { id: Tab; label: string; icon: string }[] = [
    { id: "friends", label: "Friends", icon: "M4 2h5v1h1v4H9v1H4V7H3V3h1zm-2 8h9v1h2v4H0v-4h2zm10-6h2v1h1v3h-1v1h-2V8h1V5h-1z" },
    { id: "inbox", label: "Inbox", icon: "M2 2h12v11h-3v2H5v-2H2zm2 2v7h8V4zm1 1h6v2H5zm0 3h4v2H5z" },
    { id: "account", label: "Account", icon: "M5 1h6v2h1v5h-1v1H5V8H4V3h1zm-1 10h8v1h2v3H2v-3h2z" },
]
const TITLES: Record<Tab, string> = { friends: "Friends", inbox: "Inbox", account: "Account" }
const shown = computed(() => {
    if (props.account.status !== "signedIn") return ""
    return props.account.name?.trim() || props.account.username
})
</script>

<template>
    <section class="acct panel" :class="{ hub: account.status === 'signedIn' }" aria-label="Account and friends">
        <template v-if="account.status === 'signedIn'">
            <header class="me">
                <span class="mark">{{ account.username.slice(0, 1).toUpperCase() }}</span>
                <span class="who"><strong>{{ shown }}</strong>
                    <small><span class="dot open" :class="{ stale: social?.problem }" />{{ social?.problem ? "Out of date" : "Online" }}</small></span>
            </header>
            <nav class="rail" role="tablist" aria-label="Account sections">
                <button v-for="t in TABS" :key="t.id" role="tab" :aria-selected="tab === t.id" :aria-label="t.label"
                    :class="{ on: tab === t.id, foot: t.id === 'account' }" :title="t.label" @click="tab = t.id">
                    <svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" :d="t.icon" /></svg>
                    <b v-if="t.id === 'inbox' && waiting" class="count">{{ waiting }}</b>
                </button>
            </nav>
            <div class="content">
                <h2 class="title">{{ TITLES[tab] }}</h2>
                <p v-if="error" class="error" role="alert">{{ error }}</p>
                <p v-if="social?.problem" class="stale-note">Friends may be out of date: {{ social.problem }}</p>
                <div class="page">
                    <Friends v-if="tab === 'friends'" :social="social" :busy="busy" :joining="joining"
                        @request="emit('request', $event)" @join="emit('join', $event)" @invite="emit('invite', $event)" @remove="emit('remove', $event)" />
                    <Inbox v-else-if="tab === 'inbox'" :social="social" :busy="busy" :joining="joining"
                        @join="emit('join', $event)" @dismiss="emit('dismiss', $event)" @accept="emit('accept', $event)" @remove="emit('remove', $event)" />
                    <AccountPage v-else :account="account" :busy="busy" @sign-out="emit('signOut')" @leave="emit('leave')" @open-url="emit('openUrl', $event)" />
                </div>
            </div>
        </template>
        <SignIn v-else :account="account" :remembered="remembered" :error="error" :busy="busy"
            @sign-in="emit('signIn')" @cancel="emit('cancel')" @retry="emit('retry')"
            @sign-out="emit('signOut')" @open-url="emit('openUrl', $event)" @use="emit('use', $event)" @forget="emit('forget', $event)" />
    </section>
</template>

<style scoped>
/* the inventory's glass, a little deeper: it opens over the menu's title as well as the world */
.panel { width: min(24rem, calc(100vw - 2.5rem)); border: 1px solid var(--glass-edge); border-radius: var(--radius);
    background: linear-gradient(rgb(231 237 244 / 0.035), rgb(231 237 244 / 0.035)), rgb(4 7 11 / 0.88);
    backdrop-filter: blur(8px); }
/* header across, the rail down the left, the page beside it */
.hub { display: grid; grid-template: auto minmax(0, 1fr) / 3rem minmax(0, 1fr); height: min(26rem, 70vh); }
.me { grid-column: 1 / -1; display: flex; align-items: center; gap: 0.65rem; padding: 0.6rem 0.75rem; border-bottom: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.025); }
.mark { display: grid; place-items: center; flex: none; width: 2rem; height: 2rem; border: 1px solid var(--edge-lit); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); color: var(--accent-hi); font: 0.85rem var(--font-display); }
.me small { display: flex; align-items: center; gap: 0.35rem; }
.me .dot.stale { background: var(--warn); }
.rail { display: flex; flex-direction: column; gap: 0.25rem; padding: 0.35rem; border-right: 1px solid var(--glass-edge); }
.rail button { position: relative; display: grid; place-items: center; height: 2.3rem; border: 1px solid transparent; border-radius: var(--radius); background: none; color: var(--ink-faint); }
.rail button:hover { color: var(--ink); background: rgb(231 237 244 / 0.06); }
.rail button.on { color: #fff; background: rgb(10 75 145 / 0.55); border-color: var(--edge-lit-hi); }
.rail button.foot { margin-top: auto; }
.rail svg { width: 1.05rem; height: 1.05rem; }
.count { position: absolute; top: 2px; right: 2px; min-width: 0.9rem; padding: 0 0.2rem; background: var(--accent-hi); color: var(--night-0); font: 0.5rem/0.9rem var(--font-display); }
.content { display: flex; flex-direction: column; min-height: 0; padding: 0.55rem 0.35rem 0.35rem; }
.title { margin: 0 0.4rem 0.45rem; color: var(--ink-dim); font: 0.6rem var(--font-display); text-transform: uppercase; letter-spacing: 0.1em; }
.error, .stale-note { margin: 0 0.4rem 0.4rem; font: 0.72rem var(--font-sans); }
.error { color: var(--warn); }
.stale-note { color: var(--ink-faint); }
.page { flex: 1; min-height: 0; overflow-y: auto; }
</style>
