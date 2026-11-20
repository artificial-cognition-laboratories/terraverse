<script setup lang="ts">
import type { Account } from "../bridge"
import { Button, Notice, Panel } from "../kit"

/**
 * The ArcLabs account, in whatever state it's in. Shows and asks; the owner
 * sends the intents (so every state renders in the gallery).
 */
defineProps<{ account: Account }>()
defineEmits<{ signIn: []; cancel: []; signOut: []; retry: []; open: [url: string] }>()
</script>

<template>
    <Panel title="ArcLabs account" class="account">
        <template v-if="account.status === 'signedOut'">
            <Notice v-if="account.problem" tone="warn">{{ account.problem }}</Notice>
            <p>Sign in to play with friends. Accounts are free, and singleplayer never needs one.</p>
            <Button variant="primary" @click="$emit('signIn')">Sign in »</Button>
        </template>

        <template v-else-if="account.status === 'starting'">
            <p>Asking ArcLabs for a sign-in code…</p>
        </template>

        <template v-else-if="account.status === 'pending'">
            <p>Approve this code in the browser window that just opened.</p>
            <output class="code">{{ account.code }}</output>
            <p class="dim">Check the page shows the same code. This waits until you approve.</p>
            <div class="row">
                <Button size="sm" @click="$emit('open', account.url)">Open browser</Button>
                <Button size="sm" @click="$emit('cancel')">Cancel</Button>
            </div>
        </template>

        <template v-else-if="account.status === 'checking'">
            <p>Checking your sign-in…</p>
        </template>

        <template v-else-if="account.status === 'signedIn'">
            <p class="who"><span class="chev">»</span> {{ account.username }}</p>
            <p v-if="account.name" class="dim">{{ account.name }}</p>
            <Button size="sm" @click="$emit('signOut')">Sign out</Button>
        </template>

        <template v-else>
            <Notice tone="warn">Couldn't check your sign-in: {{ account.problem }}.</Notice>
            <div class="row">
                <Button size="sm" variant="primary" @click="$emit('retry')">Retry</Button>
                <Button size="sm" @click="$emit('signOut')">Sign out</Button>
            </div>
        </template>
    </Panel>
</template>

<style scoped>
.account { width: 340px; }
.account :deep(.body) { display: grid; gap: 14px; }
p { margin: 0; font-size: 13px; line-height: 1.5; }
.dim { color: var(--ink-dim); font-size: 12px; }
.row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.row > :deep(.btn) { min-width: 0; }

.code {
    display: block;
    padding: 12px;
    text-align: center;
    font: 28px var(--font-display);
    letter-spacing: 0.18em;
    color: var(--accent-hi);
    background: var(--stone-1);
    border: var(--b) solid var(--edge);
    box-shadow: var(--sunk);
}

.who { font: 18px var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; }
.chev { color: var(--accent); }
</style>
