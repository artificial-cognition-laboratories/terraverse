<script setup lang="ts">
import { computed, ref } from "vue"
import type { Friend, Player, Social } from "../bridge"
import { Button, Field, Notice, Panel, Section, Toggle } from "../kit"
import { byPresence, presenceDot, presenceLine } from "./presence"

/**
 * Friends, requests and invites. Shows and asks; the owner sends the intents
 * and passes back the backend's refusal as `error`.
 */
const props = defineProps<{ social: Social; error: string | null; busy: boolean; joining: string | null }>()
const emit = defineEmits<{
    request: [username: string]; accept: [id: string]; remove: [id: string]
    invite: [id: string]; dismiss: [id: string]; open: [open: boolean]; close: []
    join: [player: Player]
}>()

const username = ref("")
// Removing a friend takes a second press on the same row.
const confirming = ref<string | null>(null)

const friends = computed(() => [...(props.social.view?.friends ?? [])].sort(byPresence))
const online = computed(() => friends.value.filter((f) => f.presence).length)
const open = computed({
    get: () => props.social.me.open,
    set: (value: boolean) => emit("open", value),
})

function add() {
    const name = username.value.trim()
    if (!name) return
    emit("request", name)
    username.value = ""
}

function remove(friend: Friend) {
    if (confirming.value !== friend.id) {
        confirming.value = friend.id
        return
    }
    confirming.value = null
    emit("remove", friend.id)
}

// The friend is somewhere the player could be invited from: online, not already here.
const invitable = (friend: Friend) =>
    props.social.joinable && friend.presence !== null && friend.presence.world !== props.social.me.world
const joinable = (friend: Friend) => friend.presence?.state === "hosting" && friend.presence.open
// Joining leaves where the player is: from the menu only.
const inMenu = computed(() => props.social.me.state === "menu")
const joinTitle = computed(() => (inMenu.value ? undefined : "Leave your world first"))
</script>

<template>
    <Panel title="Friends" class="friends">
        <template #actions>
            <span class="count">{{ online }} online</span>
            <Button size="sm" @click="$emit('close')">✕</Button>
        </template>

        <Toggle v-if="social.joinable" v-model="open" :label="`Friends can join ${social.me.world}`" />

        <form class="add" @submit.prevent="add">
            <Field v-model="username" label="Add a friend" placeholder="their ArcLabs username" :maxlength="64">
                <template #action><Button size="sm" variant="primary" :disabled="busy || !username.trim()">Add</Button></template>
            </Field>
        </form>
        <Notice v-if="error" tone="danger">{{ error }}</Notice>
        <Notice v-if="social.problem" tone="warn">Not up to date: {{ social.problem }}</Notice>

        <p v-if="!social.view" class="dim">Loading friends…</p>
        <template v-else>
            <Section v-if="social.view.invites.length" title="Invites">
                <div v-for="invite in social.view.invites" :key="invite.id" class="row">
                    <span class="who"><strong>{{ invite.from.username }}</strong> invited you to {{ invite.world }}</span>
                    <Button size="sm" variant="primary" :disabled="busy || !inMenu" :title="joinTitle" @click="$emit('join', invite.from)">
                        {{ joining === invite.from.id ? "Joining…" : "Join" }}
                    </Button>
                    <Button size="sm" :disabled="busy" @click="$emit('dismiss', invite.id)">✕</Button>
                </div>
            </Section>

            <Section v-if="social.view.incoming.length || social.view.outgoing.length" title="Requests">
                <div v-for="player in social.view.incoming" :key="player.id" class="row">
                    <span class="who"><strong>{{ player.username }}</strong> wants to be friends</span>
                    <Button size="sm" variant="primary" :disabled="busy" @click="$emit('accept', player.id)">Accept</Button>
                    <Button size="sm" :disabled="busy" @click="$emit('remove', player.id)">Decline</Button>
                </div>
                <div v-for="player in social.view.outgoing" :key="player.id" class="row">
                    <span class="who"><strong>{{ player.username }}</strong> <span class="dim">hasn't answered</span></span>
                    <Button size="sm" :disabled="busy" @click="$emit('remove', player.id)">Cancel</Button>
                </div>
            </Section>

            <Section title="Friends" :hint="friends.length ? undefined : 'none yet'">
                <p v-if="!friends.length" class="dim">Add someone by their ArcLabs username. They'll see your request next time they play.</p>
                <div v-for="friend in friends" :key="friend.id" class="row">
                    <span class="dot" :class="presenceDot(friend.presence)" />
                    <span class="who">
                        <strong>{{ friend.username }}</strong>
                        <span class="where">{{ presenceLine(friend.presence) }}</span>
                    </span>
                    <Button v-if="joinable(friend)" size="sm" variant="primary" :disabled="busy || !inMenu" :title="joinTitle" @click="$emit('join', friend)">
                        {{ joining === friend.id ? "Joining…" : "Join" }}
                    </Button>
                    <Button v-if="invitable(friend)" size="sm" :disabled="busy" @click="$emit('invite', friend.id)">Invite</Button>
                    <Button size="sm" :variant="confirming === friend.id ? 'danger' : 'plain'" :disabled="busy"
                        :title="confirming === friend.id ? 'Press again to remove' : 'Remove friend'" @click="remove(friend)">
                        {{ confirming === friend.id ? "Remove?" : "✕" }}
                    </Button>
                </div>
            </Section>
        </template>
    </Panel>
</template>

<style scoped>
.friends { width: 400px; max-height: min(640px, calc(100vh - 100px)); }
.friends :deep(.body) { display: grid; gap: 18px; align-content: start; }
.count { font: 11px var(--font-display); text-transform: uppercase; color: var(--ink-faint); align-self: center; }
.add :deep(.btn) { height: 38px; }
.dim { margin: 0; color: var(--ink-dim); font-size: 12px; }

.row { display: flex; align-items: center; gap: 8px; min-width: 0; }
.who { flex: 1; min-width: 0; display: flex; flex-direction: column; gap: 2px; font-size: 13px; overflow: hidden; }
.who strong { font-weight: 400; color: var(--ink); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.where { font-size: 11px; color: var(--ink-dim); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }

.dot { flex: none; width: 10px; height: 10px; border: var(--b) solid var(--edge); }
.dot.open { background: var(--accent-hi); box-shadow: 0 0 0 2px rgb(0 148 210 / 0.25); }
.dot.online { background: #5fae5a; }
.dot.offline { background: var(--stone-3); }
</style>
