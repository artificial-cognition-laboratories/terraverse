<script setup lang="ts">
import type { ActionsHandle } from "./actions.ts"
import Accounts from "./Accounts.vue"

/**
 * Online, signed out: the whole screen is the way in, centred on the page with no card round it:
 * the accounts this machine remembers (one click each), then signing in with another.
 */
defineProps<{ actions: ActionsHandle }>()
</script>

<template>
    <div class="sign-in-page">
        <p class="kicker">» Online</p>
        <p class="lead">Sign in to see friends, join their worlds and find servers.</p>
        <Accounts :actions="actions" />
    </div>
</template>

<style scoped>
.sign-in-page { display: grid; justify-items: center; gap: 0.75rem; max-width: 24rem; margin: 4rem auto 0; text-align: center; }
.sign-in-page :deep(.accounts) { justify-items: stretch; width: 100%; text-align: left; }
.sign-in-page :deep(.way-in) { width: 100%; }
.kicker { margin: 0; color: var(--accent-hi); font: 0.6rem var(--font-display); text-transform: uppercase; letter-spacing: 0.08em; }
.lead { margin: 0 0 0.5rem; color: var(--ink-dim); font: 0.9rem/1.5 var(--font-sans); }
</style>
