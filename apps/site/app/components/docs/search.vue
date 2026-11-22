<script setup lang="ts">
import { search, type Hit, type Section } from "~/lib/search"

/**
 * The docs' search: the subnav's box, with the sections that match listed under
 * it. The docs are fetched the first time the box is used, never before, so a
 * page that's only read loads nothing for search. `/` focuses it from anywhere.
 */
const words = ref("")
const box = ref<{ focus: () => void; blur: () => void } | null>(null)
const sections = shallowRef<readonly Section[] | null>(null)
const open = ref(false)
const lit = ref(0)

const hits = computed((): Hit[] => (sections.value ? search.find(sections.value, words.value) : []))
watch(hits, () => (lit.value = 0))
// Typing opens it too: a box focused before the page woke up never heard the focus
watch(words, w => {
    if (w) void load()
})

async function load() {
    open.value = true
    sections.value ??= await queryCollectionSearchSections("docs")
}

async function go(hit: Hit | undefined) {
    if (!hit) return
    open.value = false
    words.value = ""
    box.value?.blur()
    await navigateTo(hit.id)
}

// Arrows move through the list, Enter opens, Escape closes: a keyboard never needs the mouse
const KEYS: Record<string, () => void> = {
    ArrowDown: () => (lit.value = Math.min(lit.value + 1, hits.value.length - 1)),
    ArrowUp: () => (lit.value = Math.max(lit.value - 1, 0)),
    Escape: () => {
        open.value = false
        box.value?.blur()
    },
}
function key(event: KeyboardEvent) {
    const action = KEYS[event.key]
    if (!action) return
    event.preventDefault()
    action()
}

function slash(event: KeyboardEvent) {
    const typing = event.target instanceof HTMLElement && event.target.closest("input, textarea, [contenteditable]")
    if (event.key !== "/" || typing) return
    event.preventDefault()
    box.value?.focus()
}
onMounted(() => window.addEventListener("keydown", slash))
onBeforeUnmount(() => window.removeEventListener("keydown", slash))
</script>

<template>
    <shared-search
        ref="box"
        v-model="words"
        class="docs-search"
        placeholder="Search the docs"
        label="Search the docs"
        @focus="load"
        @keydown="key"
        @submit="go(hits[lit])"
        @focusout="open = false"
    >
        <template #hint>
            <kbd v-if="!words">/</kbd>
        </template>
        <ul v-if="open && words" class="results" role="listbox" @mousedown.prevent>
            <li v-if="!sections" class="note">Loading the docs…</li>
            <li v-else-if="!hits.length" class="note">Nothing in the docs says that.</li>
            <li v-for="(hit, i) in hits" :key="hit.id" role="option" :aria-selected="i === lit">
                <button type="button" :class="{ lit: i === lit }" @mouseenter="lit = i" @click="go(hit)">
                    <span v-if="hit.trail" class="trail">{{ hit.trail }}</span>
                    <span class="title">{{ hit.title }}</span>
                    <span class="snippet">{{ hit.snippet }}</span>
                </button>
            </li>
        </ul>
    </shared-search>
</template>

<style scoped lang="scss">
kbd {
    padding: 0 0.35rem;
    font-family: $font-mono;
    font-size: 0.65rem;
    color: $ink-faint;
    border: 1px solid $glass-edge;
}

.results {
    position: absolute;
    top: calc(100% + 0.4rem);
    left: -1px;
    right: -1px;
    z-index: 10;
    max-height: min(28rem, 70vh);
    margin: 0;
    padding: 0.35rem;
    overflow-y: auto;
    list-style: none;
    background: rgba($night-0, 0.96);
    backdrop-filter: blur(10px);
    border: 1px solid $glass-edge;

    button {
        display: grid;
        gap: 0.2rem;
        width: 100%;
        padding: 0.6rem 0.7rem;
        font-family: $font-sans;
        text-align: left;
        background: none;
        border: none;
        box-shadow: inset 2px 0 0 transparent;
        cursor: pointer;

        &.lit {
            background: rgba($primary-ink, 0.07);
            box-shadow: inset 2px 0 0 $primary-ink;
        }
    }
}

.trail {
    font-family: $font-display;
    font-size: 0.55rem;
    text-transform: uppercase;
    color: $ink-faint;
}

.title {
    font-size: 0.88rem;
    color: $ink;
}

.snippet {
    overflow: hidden;
    font-size: 0.75rem;
    color: $ink-dim;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.note {
    padding: 0.7rem;
    font-size: 0.8rem;
    color: $ink-faint;
}
</style>
