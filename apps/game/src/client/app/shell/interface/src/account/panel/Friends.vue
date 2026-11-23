<script setup lang="ts">
import { computed, ref } from "vue"
import type { Friend, Player, Social } from "../../bridge"
import { byPresence } from "../../social/presence"
import FriendRow from "./FriendRow.vue"

/** Who you know: a line to add someone by name, then your friends, those online first. */
const props = defineProps<{ social: Social | null; busy: boolean; joining: string | null }>()
const emit = defineEmits<{ request: [username: string]; join: [player: Player]; invite: [id: string]; remove: [id: string] }>()
const typed = ref("")
const opened = ref<string | null>(null)
const view = computed(() => props.social?.view ?? null)
const friends = computed(() => [...(view.value?.friends ?? [])].sort(byPresence))
const online = computed(() => friends.value.filter((f) => f.presence))
const offline = computed(() => friends.value.filter((f) => !f.presence))
const inMenu = computed(() => props.social?.me.state === "menu")
const canInvite = (friend: Friend) => !!props.social?.joinable && friend.presence !== null && friend.presence.world !== props.social.me.world

function add() {
    const name = typed.value.trim().replace(/^@/, "")
    if (!name || props.busy) return
    emit("request", name)
    typed.value = ""
}
</script>

<template>
    <div class="friends">
        <form class="add" @submit.prevent="add">
            <span class="plus">+</span>
            <input v-model="typed" type="text" placeholder="Add a friend by username" spellcheck="false" autocomplete="off" maxlength="64">
            <button v-if="typed.trim()" class="act go" :disabled="busy">Send</button>
        </form>
        <p v-if="!view" class="empty">{{ social ? "Loading friends…" : "Connecting…" }}</p>
        <p v-else-if="!friends.length" class="empty">No friends yet. Add someone by their username above.</p>
        <template v-for="part in [{ name: 'Online', list: online }, { name: 'Offline', list: offline }]" :key="part.name">
            <template v-if="part.list.length">
                <p class="group">{{ part.name }} · {{ part.list.length }}</p>
                <ul class="rows">
                    <FriendRow v-for="friend in part.list" :key="friend.id" :friend="friend" :open="opened === friend.id"
                        :busy="busy" :joining="joining === friend.id" :in-menu="inMenu" :can-invite="canInvite(friend)"
                        @toggle="opened = opened === friend.id ? null : friend.id" @join="emit('join', friend)"
                        @invite="emit('invite', friend.id)" @remove="opened = null; emit('remove', friend.id)" />
                </ul>
            </template>
        </template>
    </div>
</template>

<style scoped>
/* the palette's search cell: the slots' hairline and lift */
.add { display: flex; align-items: center; gap: 0.5rem; height: 2.3rem; margin-bottom: 0.25rem; padding: 0 0.35rem 0 0.7rem; border: 1px solid var(--glass-edge); background: rgb(231 237 244 / 0.025); }
.add:focus-within { border-color: var(--edge-lit); }
.plus { color: var(--accent-hi); font: 0.8rem var(--font-display); }
input { flex: 1; min-width: 0; height: 100%; border: 0; outline: 0; background: none; color: var(--ink); font: 0.8rem var(--font); }
input::placeholder { color: var(--ink-faint); }
</style>
