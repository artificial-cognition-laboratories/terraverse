<script setup lang="ts">
import type { Account } from "../../bridge"
import { Button } from "../../kit"

/** Signed out, or on the way in: one card, the code big while the browser approves it, as the site shows it. */
defineProps<{ account: Exclude<Account, { status: "signedIn" }> }>()
const emit = defineEmits<{ signIn: []; cancel: []; retry: []; signOut: []; openUrl: [url: string] }>()
</script>

<template>
    <div class="sign-in">
        <template v-if="account.status === 'signedOut'">
            <h2>Play together</h2>
            <p>Sign in to add friends and join their worlds. Playing alone needs no account.</p>
            <p v-if="account.problem" class="problem">{{ account.problem }}</p>
            <Button variant="primary" @click="emit('signIn')">Sign in »</Button>
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
.pair > * { flex: 1; }
</style>
