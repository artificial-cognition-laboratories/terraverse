<script setup lang="ts">
import { computed, ref } from "vue"
import type { CSSProperties } from "vue"
import type { Friend } from "../bridge/index.ts"
import { Monogram } from "../kit/index.ts"
import { joinable, whereabouts } from "./groups.ts"

/**
 * One friend in the popover: who and where. What you can do with them (join their world, invite
 * them to yours, copy their name, remove them) is on right-click, so the row stays quiet.
 */
/** `invited`: they've invited you, so their world can be joined whether it's open to friends or not. */
const props = withDefaults(defineProps<{ friend: Friend; canInvite: boolean; busy: boolean; joining: boolean; invited?: boolean; canPage?: boolean }>(),
    { invited: false, canPage: true })
const canJoin = computed(() => props.invited || joinable(props.friend))
const emit = defineEmits<{ join: []; invite: []; remove: []; copy: []; page: [] }>()
const menu = ref(false)
const at = ref<CSSProperties>({})
// fixed to the page under ⋯ (right edges aligned), so the scrolling list never clips it
function open(e: MouseEvent) {
    // at the pointer, kept on the page: a menu's width and its rows' height, roughly, from the edges
    at.value = { top: `${Math.min(e.clientY, window.innerHeight - 190)}px`, left: `${Math.min(e.clientX, window.innerWidth - 190)}px` }
    confirming.value = false
    menu.value = true
}
function act(what: "join" | "invite") {
    close()
    if (what === "join") emit("join")
    else emit("invite")
}
const confirming = ref(false)
function close() {
    menu.value = false
    confirming.value = false
}
function pick(action: "copy" | "remove") {
    if (action === "remove" && !confirming.value) { confirming.value = true; return }
    close()
    if (action === "remove") emit("remove")
    else emit("copy")
}
</script>

<template>
    <li class="tile" :class="{ away: !friend.presence, open: menu }" @contextmenu.prevent="open">
        <span class="face">
            <Monogram :name="friend.username" :size="30" />
            <span class="dot" :class="friend.presence ? (friend.presence.state === 'menu' ? 'on' : 'playing') : 'off'" />
        </span>
        <span class="who">
            <span class="name">{{ friend.name ?? friend.username }}</span>
            <span class="where" :class="{ world: friend.presence && friend.presence.state !== 'menu' }">{{ whereabouts(friend) }}</span>
        </span>
        <span v-if="joining" class="busy">Joining…</span>
        <Teleport to="body"><div v-if="menu" class="catch" @mousedown="close" @contextmenu.prevent="close" /><ul v-if="menu" class="menu" role="menu" :style="at">
            <li v-if="canJoin"><button role="menuitem" class="lead" :disabled="busy" @click="act('join')">Join {{ friend.presence?.world ?? "their world" }} »</button></li>
            <li v-if="canInvite && friend.presence"><button role="menuitem" :disabled="busy" @click="act('invite')">Invite to your world</button></li>
            <li v-if="canJoin || (canInvite && friend.presence)" class="rule" />
            <li v-if="canPage"><button role="menuitem" @click="close(); emit('page')">View page »</button></li>
            <li><button role="menuitem" @click="pick('copy')">Copy @{{ friend.username }}</button></li>
            <li><button role="menuitem" class="danger" @click="pick('remove')">{{ confirming ? "Click again to remove" : "Remove friend" }}</button></li>
        </ul></Teleport>
    </li>
</template>

<style scoped>
/* the library's category tile: a pane of glass with one hairline, its edge lit on hover */
.tile { position: relative; display: flex; align-items: center; gap: 0.75rem; min-width: 0; padding: 0.55rem 0.6rem 0.55rem 0.65rem; border: 1px solid rgb(231 237 244 / 0.05); border-radius: var(--radius); background: rgb(231 237 244 / 0.045); }
.tile:hover, .tile.open { border-color: var(--glass-edge-hi); background: rgb(231 237 244 / 0.07); }
.tile.away > .face, .tile.away > .who { opacity: 0.5; }
.face { position: relative; display: block; flex: none; width: 30px; height: 30px; }
.dot { position: absolute; right: -3px; bottom: -3px; width: 8px; height: 8px; border: 1px solid var(--edge); border-radius: var(--radius); }
.dot.on { background: var(--accent-hi); }
.dot.playing { background: #7bd88f; }
.dot.off { background: var(--night-4); }
.who { display: flex; flex: 1; flex-direction: column; gap: 0.25rem; min-width: 0; }
.name, .where { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.name { color: var(--ink); font: 0.66rem var(--font-display); text-transform: uppercase; letter-spacing: 0.04em; }
.tile:not(:hover) .name { color: var(--ink-dim); }
.where { color: var(--ink-faint); font: 0.68rem var(--font); }
.where.world { color: #7bd88f; }
.catch { position: fixed; inset: 0; z-index: 9; }
.menu { position: fixed; z-index: 10; min-width: 11rem; margin: 0; padding: 0.25rem; list-style: none; border: 1px solid var(--glass-edge-hi); border-radius: var(--radius); background: var(--side); box-shadow: var(--sheen), 0 8px 24px rgb(0 0 0 / 0.5); }
.menu button { display: block; width: 100%; padding: 0.45rem 0.6rem; border: 0; border-radius: var(--radius); background: none; color: var(--ink-dim); font: 0.75rem var(--font); text-align: left; white-space: nowrap; }
.menu button:hover:not(:disabled) { background: rgb(99 163 233 / 0.07); color: var(--ink); }
.menu button:disabled { color: var(--ink-faint); cursor: default; }
.menu .danger { color: #e8b04a; }
.menu .lead { color: var(--accent-hi); }
.menu .rule { height: 1px; margin: 0.25rem 0.4rem; background: var(--glass-edge); }
.busy { flex: none; color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; }
</style>
