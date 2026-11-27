<script setup lang="ts">
import { computed } from "vue"
import type { Account, Player, RememberedProfile, Social } from "../../bridge"
import Friends from "./Friends.vue"
import SignIn from "./SignIn.vue"
import Switcher from "./Switcher.vue"
import Waiting from "./Waiting.vue"
import "./rows.css"

/**
 * The account and friends, a sheet down the right edge over whatever's open: who you are and the
 * accounts you can switch to, what's waiting on you, then your friends; signed out, the way in.
 * Every verb is the caller's (account/Corner.vue, the launcher's ProfileMenu): this only says what
 * was asked. `playAs` offers each remembered account its own game (the launcher's, which can run more).
 */
const props = defineProps<{
    account: Account; social: Social | null; error: string | null; busy: boolean; joining: string | null
    remembered: RememberedProfile[] | null; playAs?: boolean
}>()
const emit = defineEmits<{
    close: []; signIn: []; cancel: []; signOut: []; leave: []; retry: []; openUrl: [url: string]
    use: [name: string]; forget: [name: string]; playAs: [name: string]
    request: [username: string]; accept: [id: string]; remove: [id: string]
    invite: [id: string]; dismiss: [id: string]; join: [player: Player]
}>()
const signedIn = computed(() => props.account.status === "signedIn" ? props.account : null)
const signedOut = computed(() => props.account.status === "signedIn" ? null : props.account)
</script>

<template>
    <div class="acct">
        <div class="scrim" @pointerdown="emit('close')" />
        <section class="sheet" aria-label="Account and friends">
            <template v-if="signedIn">
                <Switcher :account="signedIn" :social="social" :remembered="remembered" :busy="busy" :play-as="playAs"
                    @use="emit('use', $event)" @leave="emit('leave')" @sign-out="emit('signOut')"
                    @open-url="emit('openUrl', $event)" @play-as="emit('playAs', $event)" @close="emit('close')" />
                <p v-if="error" class="note error" role="alert">{{ error }}</p>
                <p v-if="social?.problem" class="note">Friends may be out of date: {{ social.problem }}</p>
                <div class="body">
                    <Waiting :social="social" :busy="busy" :joining="joining"
                        @join="emit('join', $event)" @dismiss="emit('dismiss', $event)" @accept="emit('accept', $event)" @remove="emit('remove', $event)" />
                    <Friends :social="social" :busy="busy" :joining="joining"
                        @request="emit('request', $event)" @join="emit('join', $event)" @invite="emit('invite', $event)" @remove="emit('remove', $event)" />
                </div>
            </template>
            <template v-else-if="signedOut">
                <header class="bare">
                    <span>Account</span>
                    <button class="act" aria-label="Close" @click="emit('close')">Close</button>
                </header>
                <SignIn :account="signedOut" :remembered="remembered" :error="error" :busy="busy"
                    @sign-in="emit('signIn')" @cancel="emit('cancel')" @retry="emit('retry')"
                    @sign-out="emit('signOut')" @open-url="emit('openUrl', $event)" @use="emit('use', $event)" @forget="emit('forget', $event)" />
            </template>
        </section>
    </div>
</template>

<style scoped>
/* dims and frosts what's behind (≥ 40% solid: the shell blurs the frame under it, shell/host/frame_blur.h) */
.scrim { position: fixed; inset: 0; z-index: 0; background: rgb(2 4 8 / 0.45); backdrop-filter: blur(3px); -webkit-backdrop-filter: blur(3px); }
.sheet { position: fixed; top: 0; right: 0; bottom: 0; z-index: 1; display: flex; flex-direction: column;
    width: min(23rem, 100vw); border-left: 1px solid var(--glass-edge); box-shadow: var(--sheen), -24px 0 60px rgb(0 0 0 / 0.45);
    background: linear-gradient(rgb(231 237 244 / 0.03), rgb(231 237 244 / 0.03)), rgb(4 7 11 / 0.86);
    backdrop-filter: blur(14px); -webkit-backdrop-filter: blur(14px); }
.body { flex: 1; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; padding: 0.25rem 0.5rem 0.5rem; }
.note { margin: 0.5rem 1rem 0; color: var(--ink-faint); font: 0.72rem var(--font-sans); }
.note.error { color: var(--warn); }
.bare { display: flex; align-items: center; justify-content: space-between; padding: 0.9rem 1rem; border-bottom: 1px solid var(--glass-edge);
    color: var(--ink-dim); font: 0.6rem var(--font-display); text-transform: uppercase; letter-spacing: 0.1em; }
</style>
