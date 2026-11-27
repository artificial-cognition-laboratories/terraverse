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
            <Friends v-if="open" :account="store.account.value" :social="store.social.value" :library="false" play-as :at="open"
                @close="open = null" @play-as="(name) => store.playAs(name)" />
        </Teleport>
    </div>
</template>
