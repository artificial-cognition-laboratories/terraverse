<script setup lang="ts">
import type { Account, RememberedProfile } from "../../bridge"
import { Button } from "../../kit"

/**
 * Signed out, or on the way in: the accounts this machine remembers first (one click signs in,
 * no browser), then a new sign-in; the code big while the browser approves it, as the site shows it.
 */
defineProps<{ account: Exclude<Account, { status: "signedIn" }>; remembered: RememberedProfile[] | null; error: string | null; busy: boolean }>()
const emit = defineEmits<{ signIn: []; cancel: []; retry: []; signOut: []; openUrl: [url: string]; use: [name: string]; forget: [name: string] }>()
</script>

<template>
    <div class="sign-in">
        <template v-if="account.status === 'signedOut'">
            <template v-if="remembered?.length">
                <h2>Sign in as</h2>
                <ul class="profiles">
                    <li v-for="p in remembered" :key="p.name">
                        <button class="profile" :disabled="busy" @click="emit('use', p.name)">
                            <span class="mark">{{ p.handle.slice(0, 1).toUpperCase() }}</span>
                            <strong>@{{ p.handle }}</strong>
                            <span class="go">»</span>
                        </button>
                        <button class="forget" :disabled="busy" :title="`Forget @${p.handle} on this machine`" @click="emit('forget', p.name)">Forget</button>
                    </li>
                </ul>
            </template>
            <template v-else>
                <h2>Play together</h2>
                <p>Sign in to add friends and join their worlds. Playing alone needs no account.</p>
            </template>
            <p v-if="account.problem" class="problem">{{ account.problem }}</p>
            <p v-if="error" class="problem" role="alert">{{ error }}</p>
            <Button :variant="remembered?.length ? 'plain' : 'primary'" :disabled="busy" @click="emit('signIn')">
                {{ remembered?.length ? "Sign in with another account" : "Sign in »" }}</Button>
        </template>
        <template v-else-if="account.status === 'pending'">
            <h2>Approve in your browser</h2>
            <p>Check the page shows this code:</p>
            <output class="code">{{ account.code }}</output>
            <div class="pair">
                <Button size="sm" @click="emit('openUrl', account.url)">Open the page again</Button>
                <Button size="sm" variant="ghost" @click="emit('cancel')">Cancel</Button>
            </div>
        </template>
        <template v-else-if="account.status === 'unreachable'">
            <h2>Can't reach ArcLabs</h2>
            <p class="problem">{{ account.problem }}</p>
            <div class="pair">
                <Button size="sm" variant="primary" @click="emit('retry')">Try again</Button>
                <Button size="sm" variant="ghost" @click="emit('signOut')">Sign out</Button>
            </div>
        </template>
        <p v-else class="wait">{{ account.status === "starting" ? "Getting a sign-in code…" : "Checking your sign-in…" }}</p>
    </div>
</template>

<style scoped>
.sign-in { display: grid; gap: 0.8rem; padding: 1rem 0.9rem 0.9rem; }
h2 { margin: 0; color: var(--ink); font: 0.85rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; }
p { margin: 0; color: var(--ink-dim); font: 0.78rem/1.5 var(--font-sans); }
.problem { color: var(--warn); }
.code { padding: 0.8rem; border: 1px solid var(--edge-lit); background: rgb(231 237 244 / 0.025); color: var(--ink); text-align: center; letter-spacing: 0.3em; font: 1.4rem var(--font); }
.pair { display: flex; gap: 0.5rem; }
.profiles { display: grid; gap: 0.35rem; margin: 0; padding: 0; list-style: none; }
.profiles li { display: flex; gap: 0.35rem; }
.profile { display: grid; grid-template-columns: 1.8rem minmax(0, 1fr) auto; align-items: center; gap: 0.6rem; flex: 1; min-width: 0; padding: 0.45rem 0.6rem;
    border: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.025); color: var(--ink); text-align: left; }
.profile:hover:not(:disabled) { border-color: var(--edge-lit); }
.profile:hover:not(:disabled) strong, .profile:hover:not(:disabled) .go { color: var(--accent-hi); }
.mark { display: grid; place-items: center; width: 1.8rem; height: 1.8rem; border: 1px solid var(--glass-edge-hi); background: var(--accent-lo); font: 0.75rem var(--font-display); }
.profile strong { overflow: hidden; font: 400 0.72rem var(--font-display); text-overflow: ellipsis; white-space: nowrap; }
.go { color: var(--ink-faint); font: 0.8rem var(--font-display); }
.forget { flex: none; padding: 0 0.55rem; border: 1px solid var(--glass-edge); background: none; color: var(--ink-faint); font: 0.55rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; white-space: nowrap; }
.forget:hover:not(:disabled) { border-color: var(--edge-lit); color: var(--warn); }
.pair > * { flex: 1; }
</style>
