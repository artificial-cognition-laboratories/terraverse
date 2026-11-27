<script setup lang="ts">
import { computed } from "vue"
import { useStore } from "../game/store.ts"
import IconButton from "../kit/IconButton.vue"

/**
 * The account corner: who you are at a glance (signed in or not, what's waiting on you), and the
 * way to the online screen (online/Online.vue), which its place opens (the menu's, or the game's).
 */
const emit = defineEmits<{ open: [] }>()
const store = useStore()
const waiting = computed(() => {
    const view = store.social?.view
    return view ? view.incoming.length + view.invites.length : 0
})
</script>

<template>
    <div v-if="store.account" class="corner">
        <IconButton class="profile-trigger" label="Online: friends, servers and your account" @click="emit('open')">
            <svg viewBox="0 0 16 16" width="25" height="25" shape-rendering="crispEdges" aria-hidden="true">
                <path fill="currentColor" d="M5 1h6v2h2v5h-2v2H5V8H3V3h2zm1 2v2H5v2h1v1h4V7h1V5h-1V3zM4 11h8v1h2v3H2v-3h2z" />
            </svg>
            <span class="status" :class="{ online: store.account.status === 'signedIn' }" />
            <span v-if="waiting" class="badge">{{ waiting }}</span>
        </IconButton>
    </div>
</template>

<style scoped>
.corner { position: fixed; top: 20px; right: 20px; z-index: 3; }
.status { position: absolute; right: 3px; bottom: 3px; width: 8px; height: 8px; background: var(--stone-4); border: 1px solid var(--edge); border-radius: var(--radius); }
.status.online { background: var(--accent-hi); }
.badge { position: absolute; top: -7px; right: -7px; min-width: 18px; height: 18px; padding: 0 3px; background: var(--accent); border: 1px solid var(--edge); border-radius: var(--radius); font: 10px/16px var(--font-display); }
</style>
