<script setup lang="ts">
/** Under the topnav on every registry page: where you can go, and a search that takes you to Browse. */
const route = useRoute()
const words = ref(typeof route.query.q === "string" ? route.query.q : "")
watch(() => route.query.q, q => (words.value = typeof q === "string" ? q : ""))

const links = computed(() => [
    { label: "Discover", to: "/registry", on: route.path === "/registry" },
    { label: "Browse", to: "/registry/browse", on: route.path.startsWith("/registry/browse") },
])

async function submit() {
    await navigateTo({ path: "/registry/browse", query: words.value ? { q: words.value } : {} })
}
</script>

<template>
    <shared-subnav :links="links">
        <shared-search v-model="words" placeholder="Search mods, worlds, shaders, authors…" label="Search the registry" @submit="submit" />
    </shared-subnav>
</template>
