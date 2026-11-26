<script setup lang="ts">
import { ICONS } from "~/config/icons"

/**
 * Under the topnav on every registry page: where you can go, and a search that takes you to Browse.
 * On a phone the box is an icon until it's wanted, then it takes the whole bar.
 */
const route = useRoute()
const words = ref(typeof route.query.q === "string" ? route.query.q : "")
watch(() => route.query.q, q => (words.value = typeof q === "string" ? q : ""))
const searching = ref(false)
const box = ref<{ focus: () => void } | null>(null)

const links = computed(() => [
    { label: "Discover", to: "/registry", on: route.path === "/registry" },
    { label: "Browse", to: "/registry/browse", on: route.path.startsWith("/registry/browse") },
])

async function open() {
    searching.value = true
    await nextTick()
    box.value?.focus()
}

// leaving an empty box puts the links back
function leave(event: FocusEvent) {
    const into = event.relatedTarget
    if (into instanceof Node && event.currentTarget instanceof Node && event.currentTarget.contains(into)) return
    if (!words.value) searching.value = false
}

async function submit() {
    searching.value = false
    await navigateTo({ path: "/registry/browse", query: words.value ? { q: words.value } : {} })
}
</script>

<template>
    <shared-subnav :links="links" :class="{ searching }">
        <button v-if="!searching" type="button" class="find" aria-label="Search the registry" @click="open">
            <home-pixel-icon :icon="ICONS.search" :size="14" />
        </button>
        <shared-search
            ref="box"
            v-model="words"
            class="box"
            placeholder="Search mods, worlds, shaders, authors…"
            label="Search the registry"
            @submit="submit"
            @focusout="leave"
        />
    </shared-subnav>
</template>

<style scoped lang="scss">
.find {
    display: none;
}

@include mobile {
    .find {
        @include panel;
        display: grid;
        place-items: center;
        width: 2.2rem;
        height: 2.2rem;
        color: $ink-faint;
        cursor: pointer;
    }

    .box {
        display: none;
    }

    .searching {
        :deep(.links) {
            display: none;
        }

        .box {
            display: flex;
        }
    }
}
</style>
