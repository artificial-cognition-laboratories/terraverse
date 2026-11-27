<script setup lang="ts">
import { computed, ref } from "vue"
import type { Account, RememberedProfile, Social } from "../../bridge"

/**
 * The sheet's head: who you are, and opened, the other accounts this machine remembers (switch to
 * one, or with `playAs` start a game as it beside this one), adding another, managing this one and
 * signing out. Adding leaves this account remembered and goes to the way in.
 */
const props = defineProps<{ account: Extract<Account, { status: "signedIn" }>; social: Social | null; remembered: RememberedProfile[] | null; busy: boolean; playAs?: boolean }>()
const emit = defineEmits<{ use: [name: string]; leave: []; signOut: []; openUrl: [url: string]; playAs: [name: string]; close: [] }>()
// The site the account lives on (apps/site): your name, your mods
const SITE = "https://terraverse.arclabs.it"
const open = ref(false)
const others = computed(() => (props.remembered ?? []).filter((p) => p.handle !== props.account.username))
const shown = computed(() => props.account.name?.trim() || props.account.username)
</script>

<template>
    <header class="switcher" :class="{ open }">
        <div class="me">
            <button class="who-button" :aria-expanded="open" aria-controls="accounts" @click="open = !open">
                <span class="mark">{{ account.username.slice(0, 1).toUpperCase() }}</span>
                <span class="who"><strong>{{ shown }}</strong>
                    <small><span class="dot open" :class="{ stale: social?.problem }" />@{{ account.username }} · {{ social?.problem ? "out of date" : "online" }}</small></span>
                <span class="caret" aria-hidden="true">{{ open ? "▴" : "▾" }}</span>
            </button>
            <button class="close" aria-label="Close" title="Close" @click="emit('close')">
                <svg viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M3 3h2v1h1v1h1v1h2V5h1V4h1V3h2v2h-1v1h-1v1h-1v2h1v1h1v1h1v2h-2v-1h-1v-1h-1v-1H7v1H6v1H5v1H3v-2h1v-1h1V9h1V7H5V6H4V5H3z" /></svg>
            </button>
        </div>
        <div v-if="open" id="accounts" class="accounts">
            <p v-if="others.length" class="group">Switch to</p>
            <ul v-if="others.length" class="rows">
                <li v-for="p in others" :key="p.name" class="row">
                    <span class="mark small">{{ p.handle.slice(0, 1).toUpperCase() }}</span>
                    <span class="who"><strong>@{{ p.handle }}</strong></span>
                    <button v-if="playAs" class="act" :disabled="busy" :title="`Start another game signed in as @${p.handle}`" @click="emit('playAs', p.name)">Play as</button>
                    <button class="act go" :disabled="busy" @click="emit('use', p.name)">Switch</button>
                </li>
            </ul>
            <ul class="rows">
                <li><button class="row" :disabled="busy" @click="emit('leave')">
                    <span class="plus">+</span><span class="who"><strong>Add an account</strong><small>@{{ account.username }} stays remembered</small></span>
                </button></li>
                <li><button class="row" @click="emit('openUrl', SITE)">
                    <span class="plus">»</span><span class="who"><strong>Manage your account</strong><small>terraverse.arclabs.it</small></span>
                </button></li>
                <li><button class="row" :disabled="busy" @click="emit('signOut')">
                    <span class="plus warn">×</span><span class="who"><strong class="warn">Sign out</strong><small>Forgets @{{ account.username }} on this machine</small></span>
                </button></li>
            </ul>
        </div>
    </header>
</template>

<style scoped>
.switcher { border-bottom: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.025); }
.me { display: flex; align-items: center; gap: 0.4rem; padding: 0.6rem 0.6rem 0.6rem 0.5rem; }
.who-button { flex: 1; min-width: 0; display: flex; align-items: center; gap: 0.7rem; padding: 0.35rem 0.5rem; border: 1px solid transparent; border-radius: var(--radius); background: none; color: var(--ink); text-align: left; }
.who-button:hover, .open .who-button { border-color: var(--glass-edge); background: rgb(231 237 244 / 0.05); }
.mark { display: grid; place-items: center; flex: none; width: 2.3rem; height: 2.3rem; border: 1px solid var(--edge-lit); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); color: var(--accent-hi); font: 0.95rem var(--font-display); }
.mark.small { width: 1.7rem; height: 1.7rem; font-size: 0.7rem; }
.who small { display: flex; align-items: center; gap: 0.35rem; }
.dot.stale { background: var(--warn); }
.caret { color: var(--ink-faint); font: 0.85rem var(--font-sans); }
.close { display: grid; place-items: center; flex: none; width: 2rem; height: 2rem; border: 1px solid transparent; border-radius: var(--radius); background: none; color: var(--ink-faint); }
.close:hover { color: var(--ink); border-color: var(--glass-edge); }
.close svg { width: 0.9rem; height: 0.9rem; }
.accounts { padding: 0 0.5rem 0.5rem; }
.plus { flex: none; width: 1.7rem; text-align: center; color: var(--accent-hi); font: 0.8rem var(--font-display); }
.warn { color: var(--warn); }
</style>
