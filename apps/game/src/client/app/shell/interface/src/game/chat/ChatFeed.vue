<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { useStore } from "../store.ts"

/**
 * Chat, as it comes: the last few lines in the bottom left, each going after a while; with the
 * palette open, the last screenful, held. No card: words on the world, with the night's halo.
 */
const store = useStore()
const SHOWN_FOR = 10_000 // ms a line stays, the palette shut
const HELD = 12 // lines shown while the palette's open
const RECENT = 6 // at most, shut

const now = ref(Date.now())
let ticking: ReturnType<typeof setInterval> | undefined
// a clock only while a line is waiting to go: nothing repaints for a feed that's empty
watch(() => store.chat.length, () => {
    now.value = Date.now()
    if (ticking) return
    ticking = setInterval(() => {
        now.value = Date.now()
        if (store.chat.every((line) => now.value - line.at > SHOWN_FOR)) {
            clearInterval(ticking)
            ticking = undefined
        }
    }, 500)
})
onBeforeUnmount(() => clearInterval(ticking))

const open = computed(() => store.screen === "palette")
const lines = computed(() => open.value
    ? store.chat.slice(-HELD)
    : store.chat.slice(-RECENT).filter((line) => now.value - line.at < SHOWN_FOR))
</script>

<template>
    <ol v-if="lines.length" class="chat-feed" :class="{ open }" aria-live="polite">
        <li v-for="(line, i) in lines" :key="line.at + ':' + i" :class="line.tone">
            <span v-if="line.from" class="from">{{ line.from }}</span>{{ line.text }}
        </li>
    </ol>
</template>

<style scoped>
.chat-feed { position: fixed; left: 1rem; bottom: 1rem; display: flex; flex-direction: column; gap: 0.15rem; max-width: min(34rem, 40vw); margin: 0; padding: 0; list-style: none; pointer-events: none;
    text-shadow: 0 0 3px var(--night-0), 0 0 6px var(--night-0), 0 1px 2px rgb(0 0 0 / 0.8); }
li { color: var(--ink); font: 0.8rem/1.45 var(--font-sans); white-space: pre-wrap; overflow-wrap: anywhere; }
.from { margin-right: 0.5rem; color: var(--accent-hi); font: 0.75rem var(--font); }
li.server { color: var(--ink-dim); font-family: var(--font); font-size: 0.75rem; }
li.announce { color: var(--warn); }
</style>
