<script setup lang="ts">
import { computed, ref } from "vue"
import ProfileDropdown from "@game/account/ProfileDropdown.vue"
import type { Player } from "@game/bridge/types.ts"
import { IconButton } from "@game/kit/index.ts"
import { useStore } from "../use.ts"
import { useDismiss } from "./dismiss.ts"

/** The game's account and friends dropdown, opened from the foot of the rail. */
const store = useStore()
const { state } = store
const open = ref(false)
const busy = ref(false)
const root = ref<HTMLElement | null>(null)
const menu = ref<HTMLElement | null>(null)
useDismiss(root, open, menu)

const signedIn = computed(() => store.account.value?.status === "signedIn")
const waiting = computed(() => {
    const view = store.social.value?.view
    return view ? view.incoming.length + view.invites.length : 0
})

// the rail scrolls, which would clip the dropdown: it's drawn on the body instead,
// beside the button, its foot level with the button's
const at = ref({ left: 0, bottom: 0 })
function toggle() {
    // the rail's own edge, not the button's: the same gutter beside it as the rail keeps round its icons
    const rail = root.value?.closest("nav")?.getBoundingClientRect()
    const box = root.value?.getBoundingClientRect()
    if (rail && box) at.value = { left: rail.right + 14, bottom: Math.max(8, window.innerHeight - box.bottom) }
    open.value = !open.value
}

async function act(action: () => Promise<unknown>) {
    busy.value = true
    try { await action() } finally { busy.value = false }
}
</script>

<template>
    <div ref="root" class="profile">
        <IconButton label="Account and friends" :on="open" :aria-expanded="open" @click="toggle">
            <svg viewBox="0 0 16 16" width="25" height="25" shape-rendering="crispEdges" aria-hidden="true">
                <path fill="currentColor" d="M5 1h6v2h2v5h-2v2H5V8H3V3h2zm1 2v2H5v2h1v1h4V7h1V5h-1V3zM4 11h8v1h2v3H2v-3h2z" />
            </svg>
            <span class="status" :class="{ online: signedIn }" />
            <span v-if="waiting" class="badge">{{ waiting }}</span>
        </IconButton>

        <Teleport to="body">
            <div v-if="open && store.account.value" ref="menu" class="profile-popover" :style="{ left: `${at.left}px`, bottom: `${at.bottom}px` }">
                <ProfileDropdown :account="store.account.value" :social="store.social.value" :error="state.refusal" :busy="busy" :joining="null"
                    @sign-in="act(store.signIn)" @cancel="act(store.cancelSignIn)" @retry="act(store.retrySignIn)"
                    @sign-out="act(store.signOut)" @open-url="act(() => store.openUrl($event))"
                    @request="act(() => store.requestFriend($event))" @accept="act(() => store.acceptFriend($event))"
                    @remove="act(() => store.removeFriend($event))"
                    @invite="store.notInLauncher('Inviting')" @dismiss="store.notInLauncher('Dismissing an invite')"
                    @open-world="store.notInLauncher('Opening your world')"
                    @join="(player: Player) => store.notInLauncher(`Joining ${player.username}`)" />
            </div>
        </Teleport>
    </div>
</template>

<style scoped>
.profile { position: relative; }
.status { position: absolute; right: 3px; bottom: 3px; width: 8px; height: 8px; background: var(--stone-4); border: 1px solid var(--edge); }
.status.online { background: var(--accent-hi); }
.badge { position: absolute; top: -7px; right: -7px; min-width: 18px; height: 18px; padding: 0 3px; background: var(--accent); border: 1px solid var(--edge); font: 10px/16px var(--font-display); }
.profile-popover { position: fixed; z-index: 30; }
/* the dropdown hangs below the game's corner button; here it rises from the rail's foot */
.profile-popover :deep(.dropdown) { margin-top: 0; max-height: calc(100vh - 16px); }
</style>
