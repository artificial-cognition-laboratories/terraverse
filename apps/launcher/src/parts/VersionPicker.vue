<script setup lang="ts">
import { computed, ref } from "vue"
import { useStore } from "../use.ts"
import { useDismiss } from "./dismiss.ts"

/** What Play launches: the newest, or a pinned version. Opens upward from the play bar. */
const store = useStore()
const { state } = store
const open = ref(false)
const root = ref<HTMLElement | null>(null)
useDismiss(root, open)

const pinned = computed(() => state.settings?.pinned ?? null)
const newest = computed(() => store.visible.value.find(v => v.channel === "stable" || store.betaLatest.value) ?? null)

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
            <span class="caret" aria-hidden="true">▴</span>
        </button>
        <div v-if="open" class="menu" role="listbox">
            <button role="option" :aria-selected="!pinned" :class="{ on: !pinned }" @click="choose(null)">
                <span class="name">Latest</span>
                <small>Always the newest{{ store.betaLatest.value ? ", betas included" : " stable" }}{{ newest ? ` · ${newest.id}` : "" }}</small>
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
.picker { position: relative; min-width: 0; }
.current {
    display: grid; grid-template-columns: auto auto; grid-template-rows: auto auto; align-items: center; column-gap: 14px;
    min-width: 220px; height: 56px; padding: 0 14px; text-align: left;
    border: var(--b) solid var(--edge); background: var(--stone-2); box-shadow: var(--raised), var(--ledge);
}
.current:hover, .current.open { background: var(--stone-3); }
.kind { grid-column: 1; font: 9px var(--font-display); letter-spacing: 0.14em; text-transform: uppercase; color: var(--accent-hi); }
.id { grid-column: 1; font: 700 14px var(--font); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.caret { grid-column: 2; grid-row: 1 / 3; justify-self: end; color: var(--ink-dim); font-size: 14px; }

.menu {
    position: absolute; right: 0; bottom: calc(100% + 8px); z-index: 5; display: flex; flex-direction: column;
    width: 300px; max-height: 360px; overflow-y: auto; padding: 6px;
    border: var(--b) solid var(--edge); background: var(--stone-2); box-shadow: var(--raised), 6px 6px 0 rgb(0 0 0 / 0.4);
}
.menu button {
    display: flex; flex-direction: column; align-items: flex-start; gap: 1px; padding: 7px 10px; text-align: left;
    border: var(--b) solid transparent; background: none;
}
.menu button:hover { background: var(--stone-3); }
.menu button.on { border-color: var(--edge); background: var(--accent-lo); box-shadow: var(--sunk); }
.name { font: 700 13px var(--font); }
small { font-size: 11px; color: var(--ink-dim); }
.beta { margin-right: 6px; padding: 0 4px; background: var(--warn-lo); color: var(--ink); font: 9px var(--font-display); text-transform: uppercase; }
.rule { height: 1px; margin: 4px 2px; background: var(--stone-4); }
</style>
