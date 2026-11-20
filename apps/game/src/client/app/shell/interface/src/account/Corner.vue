<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from "vue"
import { BridgeError, type IntentKind, type Intents, type Player } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import ProfileDropdown from "./ProfileDropdown.vue"
import IconButton from "../kit/IconButton.vue"

const bridge = useBridge()
const store = useStore()
const root = ref<HTMLElement | null>(null)
const expanded = ref(false)
const error = ref<string | null>(null)
const busy = ref(false)
const joining = ref<string | null>(null)
const waiting = computed(() => {
    const view = store.social?.view
    return view ? view.incoming.length + view.invites.length : 0
})

function onPointerDown(event: PointerEvent) {
    if (root.value && !root.value.contains(event.target as Node)) expanded.value = false
}
function onKeyDown(event: KeyboardEvent) {
    if (event.key === "Escape" && expanded.value) {
        event.preventDefault()
        event.stopPropagation()
        expanded.value = false
        root.value?.querySelector<HTMLButtonElement>(".profile-trigger")?.focus()
    }
}
onMounted(() => {
    document.addEventListener("pointerdown", onPointerDown)
    document.addEventListener("keydown", onKeyDown, true)
})
onUnmounted(() => {
    document.removeEventListener("pointerdown", onPointerDown)
    document.removeEventListener("keydown", onKeyDown, true)
})

async function act<K extends IntentKind>(kind: K, request: Intents[K]["request"]) {
    error.value = null
    busy.value = true
    try {
        await bridge.send(kind, request)
    } catch (e) {
        if (!(e instanceof BridgeError) || !e.refused) throw e
        error.value = e.message
    } finally {
        busy.value = false
    }
}

async function join(player: Player) {
    joining.value = player.id
    try {
        await act("social.join", { id: player.id, name: player.username })
    } finally {
        joining.value = null
    }
}
</script>

<template>
    <div v-if="store.account" ref="root" class="corner">
        <IconButton class="profile-trigger" label="Account and friends" :on="expanded"
            aria-controls="profile-dropdown" :aria-expanded="expanded" @click="expanded = !expanded">
            <svg viewBox="0 0 16 16" width="25" height="25" shape-rendering="crispEdges" aria-hidden="true">
                <path fill="currentColor" d="M5 1h6v2h2v5h-2v2H5V8H3V3h2zm1 2v2H5v2h1v1h4V7h1V5h-1V3zM4 11h8v1h2v3H2v-3h2z" />
            </svg>
            <span class="status" :class="{ online: store.account.status === 'signedIn' }" />
            <span v-if="waiting" class="badge">{{ waiting }}</span>
        </IconButton>
        <ProfileDropdown v-if="expanded" id="profile-dropdown" :account="store.account" :social="store.social"
            :error="error" :busy="busy" :joining="joining"
            @sign-in="act('account.signIn', {})" @cancel="act('account.cancel', {})"
            @sign-out="act('account.signOut', {})" @retry="act('account.retry', {})"
            @open-url="act('open', { url: $event })"
            @request="act('social.request', { username: $event })"
            @accept="act('social.accept', { id: $event })"
            @remove="act('social.remove', { id: $event })"
            @invite="act('social.invite', { id: $event })"
            @dismiss="act('social.dismiss', { id: $event })"
            @open-world="act('social.open', { open: $event })"
            @join="join" />
    </div>
</template>

<style scoped>
.corner { position: fixed; top: 20px; right: 20px; z-index: 3; max-width: calc(100vw - 40px); }
.profile-trigger { margin-left: auto; }
.status { position: absolute; right: 3px; bottom: 3px; width: 8px; height: 8px; background: var(--stone-4); border: 1px solid var(--edge); }
.status.online { background: var(--accent-hi); }
.badge { position: absolute; top: -7px; right: -7px; min-width: 18px; height: 18px; padding: 0 3px; background: var(--accent); border: 1px solid var(--edge); font: 10px/16px var(--font-display); }
</style>
