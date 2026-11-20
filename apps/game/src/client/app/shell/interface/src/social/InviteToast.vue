<script setup lang="ts">
import { onBeforeUnmount, ref, watch } from "vue"
import type { Invite } from "../bridge"
import { useStore } from "../game/store"

/**
 * A new invite, announced once wherever the player is. Answering happens in
 * the friends panel (the menu's corner, or pause in a world).
 */
const store = useStore()
const shown = ref<Invite | null>(null)
const seen = new Set<string>()
let timer: ReturnType<typeof setTimeout> | null = null

watch(() => store.social?.view?.invites, (invites) => {
    for (const invite of invites ?? []) {
        if (seen.has(invite.id)) continue
        seen.add(invite.id)
        shown.value = invite
        if (timer) clearTimeout(timer)
        timer = setTimeout(() => (shown.value = null), 10000)
    }
}, { immediate: true })

onBeforeUnmount(() => { if (timer) clearTimeout(timer) })
</script>

<template>
    <Transition name="toast">
        <aside v-if="shown" class="toast" role="status">
            <span class="chev">»</span>
            <span class="text">
                <strong>{{ shown.from.username }}</strong> invited you to {{ shown.world }}
                <small>{{ store.mode === "game" ? "Pause to answer" : "Answer in Friends" }}</small>
            </span>
            <button aria-label="Hide" @click="shown = null">✕</button>
        </aside>
    </Transition>
</template>

<style scoped>
.toast {
    position: fixed;
    top: 20px;
    left: 50%;
    transform: translateX(-50%);
    z-index: 4;
    display: flex;
    align-items: center;
    gap: 12px;
    max-width: calc(100vw - 40px);
    padding: 10px 12px 10px 14px;
    border: var(--b) solid var(--edge);
    background: rgb(21 24 27 / 0.96);
    box-shadow: inset 0 -4px 0 var(--accent), var(--raised), 6px 6px 0 rgb(0 0 0 / 0.35);
}
.chev { color: var(--accent); font-family: var(--font-display); }
.text { display: flex; flex-direction: column; gap: 2px; min-width: 0; font-size: 13px; }
.text strong { font-weight: 400; color: var(--accent-hi); }
small { font-size: 11px; color: var(--ink-faint); }
button { border: 0; background: none; color: var(--ink-dim); padding: 0 4px; }
.toast-enter-active, .toast-leave-active { transition: opacity 160ms steps(4), transform 160ms steps(4); }
.toast-enter-from, .toast-leave-to { opacity: 0; transform: translate(-50%, -12px); }
</style>
