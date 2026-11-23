<script setup lang="ts">
import { computed, ref } from "vue"
import type { Friend } from "../../bridge"
import { presenceDot, presenceLine } from "../../social/presence"

/**
 * A friend: where they are, and opened, what can be done with them here (join their open world,
 * invite them to yours, remove them, asked twice).
 */
const props = defineProps<{ friend: Friend; open: boolean; busy: boolean; joining: boolean; inMenu: boolean; canInvite: boolean }>()
const emit = defineEmits<{ toggle: []; join: []; invite: []; remove: [] }>()
const confirming = ref(false)
const joinable = computed(() => props.friend.presence?.state === "hosting" && props.friend.presence.open)

function remove() {
    if (!confirming.value) { confirming.value = true; return }
    confirming.value = false
    emit("remove")
}
</script>

<template>
    <li class="friend-row">
        <button class="row" :class="{ open, off: !friend.presence }" :aria-expanded="open" @click="confirming = false; emit('toggle')">
            <span class="dot" :class="presenceDot(friend.presence)" />
            <span class="who">
                <strong>{{ friend.name || friend.username }}</strong>
                <small>{{ presenceLine(friend.presence) }}</small>
            </span>
        </button>
        <div v-if="open" class="actions">
            <span v-if="friend.name" class="handle">@{{ friend.username }}</span>
            <button v-if="joinable" class="act go" :disabled="busy || !inMenu" :title="inMenu ? undefined : 'Leave your world first'"
                @click="emit('join')">{{ joining ? "Joining…" : "Join »" }}</button>
            <button v-if="canInvite" class="act" :disabled="busy" @click="emit('invite')">Invite</button>
            <button class="act warn" :disabled="busy" @click="remove">{{ confirming ? "Remove them?" : "Remove" }}</button>
        </div>
    </li>
</template>

<style scoped>
.friend-row { list-style: none; }
.friend-row + .friend-row { border-top: 1px solid rgb(231 237 244 / 0.05); }
.actions { display: flex; align-items: center; justify-content: flex-end; gap: 0.35rem; padding: 0.1rem 0.6rem 0.5rem 1.85rem; background: rgb(231 237 244 / 0.06); }
.handle { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; color: var(--ink-faint); font: 0.7rem var(--font); }
</style>
