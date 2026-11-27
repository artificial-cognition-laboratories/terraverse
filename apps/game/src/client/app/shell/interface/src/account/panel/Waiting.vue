<script setup lang="ts">
import { computed } from "vue"
import type { Player, Social } from "../../bridge"

/**
 * What's waiting on you, pinned above your friends: invites to a world and friend requests, each
 * answered where it stands; then the requests you've sent. Nothing waiting, nothing drawn.
 */
const props = defineProps<{ social: Social | null; busy: boolean; joining: string | null }>()
const emit = defineEmits<{ join: [player: Player]; dismiss: [id: string]; accept: [id: string]; remove: [id: string] }>()
const view = computed(() => props.social?.view ?? null)
const inMenu = computed(() => props.social?.me.state === "menu")
const asks = computed(() => (view.value?.invites.length ?? 0) + (view.value?.incoming.length ?? 0))
</script>

<template>
    <section v-if="asks" class="waiting" aria-label="Waiting on you">
        <p class="group">Waiting on you · {{ asks }}</p>
        <ul class="rows">
            <li v-for="invite in view!.invites" :key="invite.id" class="row">
                <span class="dot open" />
                <span class="who"><strong>{{ invite.from.name || invite.from.username }}</strong><small>Invited you to {{ invite.world }}</small></span>
                <button class="act" :disabled="busy" @click="emit('dismiss', invite.id)">Dismiss</button>
                <button class="act go" :disabled="busy || !inMenu" :title="inMenu ? undefined : 'Leave your world first'"
                    @click="emit('join', invite.from)">{{ joining === invite.from.id ? "Joining…" : "Join »" }}</button>
            </li>
            <li v-for="player in view!.incoming" :key="player.id" class="row">
                <span class="dot" />
                <span class="who"><strong>{{ player.name || player.username }}</strong><small>@{{ player.username }} wants to be friends</small></span>
                <button class="act" :disabled="busy" @click="emit('remove', player.id)">Decline</button>
                <button class="act go" :disabled="busy" @click="emit('accept', player.id)">Accept</button>
            </li>
        </ul>
    </section>
    <section v-if="view?.outgoing.length" class="sent" aria-label="Requests sent">
        <p class="group">Sent · {{ view.outgoing.length }}</p>
        <ul class="rows">
            <li v-for="player in view.outgoing" :key="player.id" class="row off">
                <span class="dot" />
                <span class="who"><strong>{{ player.username }}</strong><small>Waiting for them</small></span>
                <button class="act" :disabled="busy" @click="emit('remove', player.id)">Cancel</button>
            </li>
        </ul>
    </section>
</template>

<style scoped>
/* the ocean's tint: these want an answer */
.waiting { margin: 0.5rem 0 0.25rem; border: 1px solid var(--edge-lit); border-radius: var(--radius); background: rgb(10 75 145 / 0.12); }
.waiting .group { margin-top: 0.55rem; color: var(--accent-hi); }
</style>
