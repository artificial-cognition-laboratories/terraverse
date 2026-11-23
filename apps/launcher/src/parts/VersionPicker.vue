<script setup lang="ts">
import { computed, ref } from "vue"
import { useStore } from "../use.ts"
import { useDismiss } from "./dismiss.ts"

/** What Play launches: the newest, or a pinned version. Opens downward, under the word. */
const store = useStore()
const { state } = store
const open = ref(false)
const root = ref<HTMLElement | null>(null)
useDismiss(root, open)

const pinned = computed(() => state.settings?.pinned ?? null)

async function choose(id: string | null) {
    open.value = false
    await store.pin(id)
}
</script>

<template>
    <div ref="root" class="picker">
        <button class="current" :class="{ open }" :aria-expanded="open" aria-haspopup="listbox" @click="open = !open">
            <span class="kind">{{ pinned ? "Pinned" : "Latest" }}</span>
            <span class="id">{{ store.target.value?.id ?? "—" }}</span>
            <span class="caret" aria-hidden="true">▾</span>
        </button>
        <div v-if="open" class="menu" role="listbox">
            <button role="option" :aria-selected="!pinned" :class="{ on: !pinned }" @click="choose(null)">
                <span class="name">Latest</span>
                <small>Always the newest{{ store.betaLatest.value ? ", betas included" : " stable" }}{{ store.newest.value ? ` · ${store.newest.value.id}` : "" }}</small>
            </button>
            <div class="rule" />
            <button v-for="v in store.visible.value" :key="v.id" role="option" :aria-selected="pinned === v.id"
                :class="{ on: pinned === v.id }" @click="choose(v.id)">
                <span class="name">{{ v.id }}</span>
                <small>
                    <span v-if="v.channel === 'beta'" class="beta">beta</span>
                    {{ store.standing(v.id).state === "installed" ? "installed" : "not installed" }}
                </small>
            </button>
        </div>
    </div>
</template>

<style scoped>
/* the foot bar's left (pages/Home.vue .foot), a plain glass button (kit Button, sm): which version Play takes, opening downward */
.picker { position: relative; min-width: 0; }
.current { display: flex; align-items: center; justify-content: center; gap: 10px; width: 100%; height: 2.1rem; padding: 0 0.8rem;
    border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); color: var(--ink); }
.current:hover, .current.open { border-color: var(--edge-lit); color: #fff; }
.current:focus-visible { outline: 1px solid var(--accent-hi); outline-offset: 2px; }
.kind { font: 0.55rem var(--font-display); letter-spacing: 0.08em; text-transform: uppercase; color: var(--accent-hi); }
.id { font: 700 0.75rem var(--font); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.caret { color: var(--ink-faint); font-size: 0.7rem; }
.open .caret { color: var(--ink); }

/* the game's dropdown glass: strong, one hairline, the chosen in the ocean */
.menu { position: absolute; left: 0; right: auto; width: max(100%, 280px); top: calc(100% + 8px); z-index: 5; display: flex; flex-direction: column; gap: 1px;
    max-height: min(320px, 40vh); overflow-y: auto; padding: 4px;
    border: 1px solid var(--glass-edge); background: var(--glass-strong); box-shadow: var(--sheen); backdrop-filter: blur(12px); }
.menu button { display: flex; flex-direction: column; align-items: flex-start; text-align: left; gap: 2px; padding: 8px 10px; text-align: left; border: 1px solid transparent; background: none; color: var(--ink); }
.menu button:hover { background: rgb(231 237 244 / 0.06); }
.menu button.on { border-color: var(--edge-lit-hi); background: var(--glass-lit); }
.name { font: 700 0.8rem var(--font); }
small { font: 0.7rem var(--font-sans); color: var(--ink-dim); }
.beta { margin-right: 6px; padding: 1px 5px; border: 1px solid rgb(210 153 34 / 0.5); color: var(--warn, #d29922); font: 0.55rem var(--font-display); text-transform: uppercase; }
.rule { height: 1px; margin: 4px 2px; background: var(--glass-edge); }
</style>
