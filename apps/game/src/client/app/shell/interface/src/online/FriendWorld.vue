<script setup lang="ts">
import { computed } from "vue"
import type { Friend } from "../bridge/index.ts"
import { Card } from "../kit/index.ts"
import { presenceLine } from "../social/presence.ts"
import { places } from "./places.ts"

/** A friend's world as a card: the world, who's in it, and joining it when it's open to friends. */
const props = defineProps<{ friend: Friend; joining: boolean; busy: boolean; inMenu: boolean }>()
const emit = defineEmits<{ open: []; join: [] }>()
const who = computed(() => props.friend.name || props.friend.username)
const open = computed(() => places.joinable(props.friend))
</script>

<template>
    <div class="friend-world">
        <Card :title="friend.presence?.world ?? 'A world'" :seed="friend.presence?.world ?? friend.id" :by="`${open ? 'hosted' : 'played'} by ${who}`"
            :description="presenceLine(friend.presence)" :badges="open ? ['Open'] : []" dense @open="emit('open')">
            <template #meta>
                <span class="dot" :class="{ open }" />{{ open ? "Friends can join" : "Not open to friends" }}
            </template>
        </Card>
        <button v-if="open" type="button" class="join" :disabled="busy || !inMenu" :title="inMenu ? undefined : 'Leave your world first'"
            @click="emit('join')">{{ joining ? "Joining…" : "Join »" }}</button>
    </div>
</template>

<style scoped>
.friend-world { position: relative; min-width: 0; display: grid; }
.dot { display: inline-block; width: 0.45rem; height: 0.45rem; margin-right: 0.5rem; background: var(--alive); }
.dot.open { background: var(--accent-hi); }
/* over the card's foot, its own press: the card opens the friend, this joins them */
.join { position: absolute; right: 0.75rem; bottom: 0.65rem; height: 1.7rem; padding: 0 0.7rem; border: 1px solid var(--edge-lit-hi); border-radius: var(--radius);
    background: var(--glass-lit); color: #fff; font: 0.58rem var(--font-display); text-transform: uppercase; letter-spacing: 0.06em; }
.join:disabled { opacity: 0.5; }
</style>
