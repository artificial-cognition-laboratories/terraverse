<script setup lang="ts">
import { computed } from "vue"
import type { Player, Social } from "../../bridge"

/** What's waiting on you (invites to a world, friend requests) and what you're waiting on. */
const props = defineProps<{ social: Social | null; busy: boolean; joining: string | null }>()
const emit = defineEmits<{ join: [player: Player]; dismiss: [id: string]; accept: [id: string]; remove: [id: string] }>()
const view = computed(() => props.social?.view ?? null)
const inMenu = computed(() => props.social?.me.state === "menu")
const empty = computed(() => !!view.value && !view.value.invites.length && !view.value.incoming.length && !view.value.outgoing.length)
</script>

<template>
    <div class="inbox">
        <p v-if="!view" class="empty">{{ social ? "Loading…" : "Connecting…" }}</p>
        <p v-else-if="empty" class="empty">Nothing waiting. Invites and friend requests turn up here.</p>
        <template v-if="view?.invites.length">
            <p class="group">Invites</p>
            <ul class="rows">
                <li v-for="invite in view.invites" :key="invite.id" class="row">
                    <span class="who"><strong>{{ invite.from.name || invite.from.username }}</strong><small>Invited you to {{ invite.world }}</small></span>
                    <button class="act go" :disabled="busy || !inMenu" :title="inMenu ? undefined : 'Leave your world first'"
                        @click="emit('join', invite.from)">{{ joining === invite.from.id ? "Joining…" : "Join" }}</button>
                    <button class="act" :disabled="busy" @click="emit('dismiss', invite.id)">Dismiss</button>
                </li>
            </ul>
        </template>
        <template v-if="view?.incoming.length">
            <p class="group">Friend requests</p>
            <ul class="rows">
                <li v-for="player in view.incoming" :key="player.id" class="row">
                    <span class="who"><strong>{{ player.name || player.username }}</strong><small>@{{ player.username }}</small></span>
                    <button class="act go" :disabled="busy" @click="emit('accept', player.id)">Accept</button>
                    <button class="act" :disabled="busy" @click="emit('remove', player.id)">Decline</button>
                </li>
            </ul>
        </template>
        <template v-if="view?.outgoing.length">
            <p class="group">Sent</p>
            <ul class="rows">
                <li v-for="player in view.outgoing" :key="player.id" class="row off">
                    <span class="who"><strong>{{ player.username }}</strong><small>Waiting for them</small></span>
                    <button class="act" :disabled="busy" @click="emit('remove', player.id)">Cancel</button>
                </li>
            </ul>
        </template>
    </div>
</template>
