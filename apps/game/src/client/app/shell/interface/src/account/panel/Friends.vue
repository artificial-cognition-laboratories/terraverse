<script setup lang="ts">
import { computed, ref } from "vue"
import type { Friend, Player, Social } from "../../bridge"
import { byPresence } from "../../social/presence"
import FriendRow from "./FriendRow.vue"

/** Who you know, by where they are (playing, online, offline), then a line to add someone by name. */
const props = defineProps<{ social: Social | null; busy: boolean; joining: string | null }>()
const emit = defineEmits<{ request: [username: string]; join: [player: Player]; invite: [id: string]; remove: [id: string] }>()
const typed = ref("")
const view = computed(() => props.social?.view ?? null)
const friends = computed(() => [...(view.value?.friends ?? [])].sort(byPresence))
const GROUPS: readonly { name: string; holds: (f: Friend) => boolean }[] = [
    { name: "Playing", holds: (f) => f.presence !== null && f.presence.state !== "menu" },
    { name: "Online", holds: (f) => f.presence?.state === "menu" },
    { name: "Offline", holds: (f) => f.presence === null },
]
const groups = computed(() => GROUPS.map((g) => ({ name: g.name, list: friends.value.filter(g.holds) })).filter((g) => g.list.length))
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
        <p v-if="!view" class="empty">{{ social ? "Loading friends…" : "Connecting…" }}</p>
        <p v-else-if="!friends.length" class="empty">No friends yet. Add someone by their username below.</p>
        <template v-for="group in groups" :key="group.name">
            <p class="group">{{ group.name }} · {{ group.list.length }}</p>
            <ul class="rows">
                <FriendRow v-for="friend in group.list" :key="friend.id" :friend="friend"
                    :busy="busy" :joining="joining === friend.id" :in-menu="inMenu" :can-invite="canInvite(friend)"
                    @join="emit('join', friend)" @invite="emit('invite', friend.id)" @remove="emit('remove', friend.id)" />
            </ul>
        </template>
        <form class="add" @submit.prevent="add">
            <span class="plus">+</span>
            <input v-model="typed" type="text" placeholder="Add a friend by username" spellcheck="false" autocomplete="off" maxlength="64">
            <button v-if="typed.trim()" class="act go" :disabled="busy">Send</button>
        </form>
    </div>
</template>

<style scoped>
.friends { flex: 1; display: flex; flex-direction: column; }
/* at the sheet's foot, under the list however long */
.add { position: sticky; bottom: 0; display: flex; align-items: center; gap: 0.5rem; height: 2.4rem; margin-top: auto; padding: 0 0.35rem 0 0.7rem;
    border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(7 11 17 / 0.95); }
.friends > .group:first-child, .friends > .empty:first-child { margin-top: 0.6rem; }
.add:focus-within { border-color: var(--edge-lit); }
.plus { color: var(--accent-hi); font: 0.8rem var(--font-display); }
input { flex: 1; min-width: 0; height: 100%; border: 0; outline: 0; background: none; color: var(--ink); font: 0.8rem var(--font); }
input::placeholder { color: var(--ink-faint); }
.rows { margin-bottom: 0.5rem; }
</style>
