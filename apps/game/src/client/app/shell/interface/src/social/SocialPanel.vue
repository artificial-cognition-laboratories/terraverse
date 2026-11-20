<script setup lang="ts">
import { ref } from "vue"
import { BridgeError, type IntentKind, type Intents, type Player } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { useStore } from "../game/store.ts"
import FriendsPanel from "./FriendsPanel.vue"

/** The friends panel, wired: sends the player's actions and shows the backend's refusals. */
defineEmits<{ close: [] }>()
const bridge = useBridge()
const store = useStore()
const error = ref<string | null>(null)
const busy = ref(false)
const joining = ref<string | null>(null)

// One action at a time: they wait on ArcLabs, and the list refreshes after each.
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

// Getting ready takes a few seconds (STUN, a ticket); then the menu leaves for their world.
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
    <FriendsPanel
        v-if="store.social"
        :social="store.social"
        :error="error"
        :busy="busy"
        :joining="joining"
        @request="act('social.request', { username: $event })"
        @accept="act('social.accept', { id: $event })"
        @remove="act('social.remove', { id: $event })"
        @invite="act('social.invite', { id: $event })"
        @dismiss="act('social.dismiss', { id: $event })"
        @open="act('social.open', { open: $event })"
        @join="join"
        @close="$emit('close')"
    />
</template>
