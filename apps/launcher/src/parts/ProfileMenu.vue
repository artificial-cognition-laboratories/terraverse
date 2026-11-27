<script setup lang="ts">
import { ref } from "vue"
import Corner from "@game/account/Corner.vue"
import Friends from "@game/friends/Friends.vue"
import { useStore } from "../use.ts"

/**
 * The game's account corner and friends popover, the same as in the menu and in game: answered by
 * the launcher's backend (bridge.ts). No library here, so Manage is the site; a remembered account
 * can be launched beside this one (Play as). It hangs from the icon, and over the whole window.
 */
const store = useStore()
// where the icon is while the popover's open, so it hangs from it
const open = ref<DOMRect | null>(null)
</script>

<template>
    <div class="profile">
        <Corner :account="store.account.value" :social="store.social.value" @open="(at) => (open = open ? null : at)" />
        <!-- on the body: inside the stage its glass had nothing behind it to blur -->
        <Teleport to="body">
            <Friends v-if="open" class="solid-glass" :account="store.account.value" :social="store.social.value" :library="false" :at="open"
                @close="open = null" />
        </Teleport>
    </div>
</template>

<style>
/* WebKitGTK blurs what's behind at a fraction of its resolution and scales it back up, so the
   popover's frost came out as blocks over the launcher's sky: here its glass is all but solid,
   and nothing is blurred. (The game's Chromium blurs properly; its popover keeps the frost.) */
.solid-glass.scrim .pop { background: rgb(5 8 12 / 0.97); backdrop-filter: none; }
</style>
