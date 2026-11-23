<script setup lang="ts">
import type { Account } from "../../bridge"

/** Who you're signed in as, where the account is managed, and signing out. */
defineProps<{ account: Extract<Account, { status: "signedIn" }>; busy: boolean }>()
const emit = defineEmits<{ signOut: []; openUrl: [url: string] }>()
// The site the account lives on (apps/site): sign-in, your name, your mods
const SITE = "https://terraverse.arclabs.it"
</script>

<template>
    <div class="account-page">
        <dl class="facts">
            <div><dt>Username</dt><dd>@{{ account.username }}</dd></div>
            <div v-if="account.name"><dt>Name</dt><dd>{{ account.name }}</dd></div>
        </dl>
        <ul class="rows">
            <li><button class="row" @click="emit('openUrl', SITE)">
                <span class="who"><strong>Manage your account</strong><small>terraverse.arclabs.it</small></span>
                <span class="out">»</span>
            </button></li>
        </ul>
        <div class="foot">
            <button class="act warn" :disabled="busy" @click="emit('signOut')">Sign out</button>
        </div>
    </div>
</template>

<style scoped>
.facts { margin: 0 0 0.4rem; }
.facts > div { display: flex; justify-content: space-between; gap: 1rem; padding: 0.5rem 0.6rem; border-bottom: 1px solid rgb(231 237 244 / 0.05); font: 0.75rem var(--font); }
dt { color: var(--ink-faint); }
dd { margin: 0; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--ink); }
.out { color: var(--ink-faint); font: 0.8rem var(--font-display); }
.row:hover .out { color: var(--accent-hi); }
.foot { display: flex; justify-content: flex-end; margin-top: 0.6rem; padding: 0.6rem 0.6rem 0.2rem; border-top: 1px solid var(--glass-edge); }
</style>
