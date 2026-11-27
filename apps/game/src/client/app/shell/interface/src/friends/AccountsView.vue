<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { RememberedProfile } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import { Monogram, Spinner } from "../kit/index.ts"
import type { ActionsHandle } from "../online/actions.ts"

/**
 * The accounts on this machine. Signed in (`me`): the others to switch to, adding another, and at
 * the foot your account on the site and signing out (it stays remembered). Signed out: each one a
 * click to sign in as, forgettable, and signing in another. While a sign-in is under way the same
 * pane is that instead: the code to check on the page, and at the foot opening it again or cancelling.
 * `switching`: the profile being switched to, its tile spinning while the popover holds still.
 */
const props = defineProps<{ actions: ActionsHandle; me: { username: string } | null; switching: string | null }>()
const emit = defineEmits<{ switch: [name: string]; add: []; cancel: []; manage: [] }>()
const bridge = useBridge()
const store = useStore()
const remembered = ref<RememberedProfile[]>([])
const flow = computed(() => {
    const a = store.account
    return a && a.status !== "signedIn" && a.status !== "signedOut" ? a : null
})
const shown = computed(() => remembered.value.filter((p) => p.handle !== props.me?.username))
async function read() { remembered.value = await bridge.send("profiles.list", {}) }
// on arriving signed in or out, not mid-switch: the list would empty and refill under the pointer
watch(() => store.account?.status, (status) => { if (status === "signedIn" || status === "signedOut") void read() }, { immediate: true })
async function forget(name: string) {
    if (await props.actions.send("profiles.forget", { name: name })) await read()
}
</script>

<template>
    <div class="accounts">
        <div v-if="flow" class="body approve">
            <template v-if="flow.status === 'pending'">
                <p class="kicker">Approve in your browser</p>
                <output class="code">{{ flow.code }}</output>
                <p class="hint">Check the page shows this code</p>
            </template>
            <template v-else-if="flow.status === 'unreachable'">
                <p class="kicker warn">Can't reach ArcLabs</p>
                <p class="hint">{{ flow.problem }}</p>
            </template>
            <template v-else>
                <Spinner :label="flow.status === 'checking' ? 'Signing in' : 'Opening'" />
                <p class="hint">{{ flow.status === "checking" ? "Signing in…" : "Opening your browser…" }}</p>
            </template>
        </div>
        <div v-else class="body">
            <p v-if="!me && !shown.length" class="lead">Sign in to see your friends and join their worlds. Playing alone needs no account.</p>
            <template v-if="shown.length">
                <p class="label">{{ me ? "Switch to" : "Sign in as" }} · {{ shown.length }}</p>
                <div v-for="p in shown" :key="p.name" class="tile" :class="{ on: switching === p.name }">
                    <button class="pick" :disabled="actions.busy.value || !!switching" @click="emit('switch', p.name)">
                        <Monogram :name="p.handle" :size="30" />
                        <span class="who"><span class="name">{{ p.handle }}</span><span class="sub">{{ switching === p.name ? "Signing in…" : "Remembered on this machine" }}</span></span>
                        <Spinner v-if="switching === p.name" label="Signing in" />
                        <span v-else class="go">{{ me ? "Switch »" : "Sign in »" }}</span>
                    </button>
                    <button v-if="!me && !switching" class="forget" :disabled="actions.busy.value" :title="`Forget @${p.handle} on this machine`" @click="forget(p.name)">✕</button>
                </div>
            </template>
            <button class="tile add" :disabled="actions.busy.value || !!switching" @click="emit('add')">
                <span class="plus">+</span>
                <span class="who"><span class="name">{{ me ? "Add an account" : shown.length ? "Sign in with another account" : "Sign in" }}</span>
                    <span class="sub">{{ me ? `@${me.username} stays remembered` : "Opens your browser" }}</span></span>
            </button>
        </div>
        <footer v-if="flow" class="foot">
            <button v-if="flow.status === 'pending'" class="act" @click="actions.send('open', { url: flow.url })">Open again »</button>
            <button v-else-if="flow.status === 'unreachable'" class="act" @click="actions.send('account.retry', {})">Try again</button>
            <span v-else />
            <button class="act stop" @click="emit('cancel')">Cancel</button>
        </footer>
        <footer v-else-if="me" class="foot">
            <button class="act" @click="emit('manage')">Manage »</button>
            <button class="act warn" :disabled="actions.busy.value" @click="actions.send('account.leave', {})">Sign out</button>
        </footer>
    </div>
</template>

<style scoped>
.accounts { display: grid; grid-template-rows: minmax(0, 1fr) auto; min-height: 0; }
.body { display: flex; flex-direction: column; gap: 0.35rem; overflow-y: auto; padding: 0.85rem 0.6rem; }
.lead { margin: 0 0.1rem 0.6rem; color: var(--ink-dim); font: 0.75rem/1.5 var(--font); }
.label { margin: 0 0 0.2rem; padding: 0 0.1rem 0.4rem; border-bottom: 1px solid var(--glass-edge); color: var(--ink-faint); font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
.tile { position: relative; display: flex; align-items: center; min-width: 0; border: 1px solid rgb(231 237 244 / 0.05); border-radius: var(--radius); background: rgb(231 237 244 / 0.045); }
.tile:hover { border-color: var(--glass-edge-hi); background: rgb(231 237 244 / 0.07); }
.tile.on { border-color: var(--edge-lit); background: rgb(99 163 233 / 0.07); }
.pick { display: flex; flex: 1; align-items: center; gap: 0.75rem; min-width: 0; padding: 0.55rem 0.65rem; border: 0; background: none; text-align: left; }
.pick:disabled { cursor: default; }
.who { display: flex; flex: 1; flex-direction: column; gap: 0.25rem; min-width: 0; }
.name, .sub { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.name { color: var(--ink-dim); font: 0.66rem var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; }
.tile:hover .name { color: var(--ink); }
.sub { color: var(--ink-faint); font: 0.68rem var(--font); }
.go { flex: none; color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; opacity: 0; }
.tile:hover .go { opacity: 1; }
.forget { flex: none; width: 24px; height: 24px; margin-right: 0.5rem; border: 0; border-radius: var(--radius); background: none; color: var(--ink-faint); font: 0.7rem var(--font); opacity: 0; }
.tile:hover .forget { opacity: 1; }
.forget:hover { color: #e8b04a; }
.tile.add { gap: 0.75rem; padding: 0.55rem 0.65rem; border-style: dashed; border-color: rgb(99 163 233 / 0.22); text-align: left; }
.tile.add:disabled { cursor: default; }
.plus { display: grid; place-items: center; flex: none; width: 30px; height: 30px; color: var(--accent-hi); font: 0.9rem var(--font-display); }
/* the pane is the sign-in: centred in it, the code the one thing to read */
.approve { align-items: center; justify-content: center; gap: 0.4rem; padding-bottom: 2.5rem; text-align: center; }
.kicker { margin: 0; padding-left: 0.1em; color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.1em; }
.warn { color: #e8b04a; }
.code { display: block; color: var(--ink); font: 2.4rem var(--font-display); letter-spacing: 0.2em; padding-left: 0.2em; /* the spacing after the last letter, matched before the first, so it centres */ }
.hint { max-width: 15rem; margin: 0; color: var(--ink-faint); font: 0.7rem/1.5 var(--font); }
.foot { display: grid; grid-template-columns: 1fr 1fr; gap: 0.4rem; padding: 0.6rem; border-top: 1px solid var(--glass-edge); }
.act { height: 28px; border: 1px solid var(--glass-edge-hi); border-radius: var(--radius); background: none; color: var(--ink-dim); font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; white-space: nowrap; }
.act:hover:not(:disabled) { color: var(--ink); border-color: var(--edge-lit-hi); }
.act.warn { color: #e8b04a; }
/* as the main menu's Quit: a red edge, quietly */
.act.stop { border-color: rgb(248 81 73 / 0.3); }
.act.stop:hover { border-color: rgb(248 81 73 / 0.6); background: rgb(110 31 27 / 0.3); color: var(--ink); }
.act.warn:hover:not(:disabled) { border-color: rgb(232 176 74 / 0.5); }
</style>
