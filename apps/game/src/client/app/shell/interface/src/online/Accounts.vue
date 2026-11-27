<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { RememberedProfile } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import { Button, Monogram, Notice } from "../kit/index.ts"
import SignIn from "../account/panel/SignIn.vue"
import type { ActionsHandle } from "./actions.ts"

/**
 * The accounts this machine remembers: signed in, the others to switch to, adding another (this one
 * stays remembered) and signing out; signed out, the way in (the remembered first, one click each).
 */
const props = defineProps<{ actions: ActionsHandle }>()
const bridge = useBridge()
const store = useStore()
const remembered = ref<RememberedProfile[] | null>(null)
const me = computed(() => store.account?.status === "signedIn" ? store.account : null)
const out = computed(() => store.account && store.account.status !== "signedIn" ? store.account : null)
const others = computed(() => (remembered.value ?? []).filter((p) => p.handle !== me.value?.username))
// read again whenever the account changes: switching, adding and forgetting all change the list
watch(() => store.account?.status, async () => { remembered.value = await bridge.send("profiles.list", {}) }, { immediate: true })
async function forget(name: string) {
    if (await props.actions.send("profiles.forget", { name: name })) remembered.value = await bridge.send("profiles.list", {})
}
const SITE = "https://terraverse.arclabs.it"
</script>

<template>
    <section class="accounts">
        <Notice v-if="actions.problem.value" tone="danger">{{ actions.problem.value }}</Notice>
        <template v-if="me">
            <ul class="list">
                <li class="current">
                    <Monogram :name="me.username" :size="36" />
                    <span class="who"><strong>@{{ me.username }}</strong><small>Signed in here</small></span>
                    <Button size="sm" @click="actions.send('open', { url: SITE })">Manage »</Button>
                    <Button size="sm" :disabled="actions.busy.value" @click="actions.send('account.signOut', {})">Sign out</Button>
                </li>
                <li v-for="p in others" :key="p.name">
                    <Monogram :name="p.handle" :size="36" />
                    <span class="who"><strong>@{{ p.handle }}</strong><small>Remembered on this machine</small></span>
                    <Button size="sm" :disabled="actions.busy.value" @click="forget(p.name)">Forget</Button>
                    <Button variant="primary" size="sm" :disabled="actions.busy.value" @click="actions.send('profiles.use', { name: p.name })">Switch »</Button>
                </li>
            </ul>
            <Button :disabled="actions.busy.value" @click="actions.send('account.leave', {})">+ Add an account</Button>
            <p class="note">Adding keeps @{{ me.username }} remembered: switch back in one click. Signing out forgets it here; its worlds stay.</p>
        </template>
        <div v-else-if="out" class="way-in">
            <SignIn :account="out" :remembered="remembered" :error="actions.problem.value" :busy="actions.busy.value"
                @sign-in="actions.send('account.signIn', {})" @cancel="actions.send('account.cancel', {})" @retry="actions.send('account.retry', {})"
                @sign-out="actions.send('account.signOut', {})" @open-url="(url) => actions.send('open', { url: url })"
                @use="(name) => actions.send('profiles.use', { name: name })" @forget="forget" />
        </div>
    </section>
</template>

<style scoped>
.accounts { display: grid; gap: 1rem; justify-items: start; }
.list { display: grid; width: 100%; margin: 0; padding: 0; list-style: none; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); }
.list li { display: flex; align-items: center; gap: 0.9rem; padding: 0.85rem 1rem; }
.list li + li { border-top: 1px solid var(--glass-edge); }
.list .current { background: rgb(10 75 145 / 0.12); }
.who { flex: 1; display: grid; gap: 0.2rem; min-width: 0; }
.who strong { overflow: hidden; color: var(--ink); font: 700 0.85rem var(--font); text-overflow: ellipsis; white-space: nowrap; }
.who small { color: var(--ink-faint); font: 0.72rem var(--font-sans); }
.note { margin: 0; color: var(--ink-faint); font: 0.75rem/1.5 var(--font-sans); }
.way-in { width: min(24rem, 100%); }
</style>
