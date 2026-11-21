<script setup lang="ts">
import { useRegistry } from "~/registry"

definePageMeta({ layout: "landing" })

// The address is the mod's name: /registry/@scope/name
const route = useRoute()
const name = computed(() => `${route.params.scope}/${route.params.name}`)
const reg = useRegistry()
const { data: mod } = await useAsyncData(() => `mod:${name.value}`, () => reg.mod(name.value))
if (!String(route.params.scope).startsWith("@") || !mod.value) {
    throw createError({ statusCode: 404, statusMessage: `There's no ${name.value} in the registry.`, fatal: true })
}
useHead({ title: () => mod.value?.title ?? "Registry" })
useSeoMeta({ description: () => mod.value?.description ?? "" })
</script>

<template>
    <registry-shell>
        <registry-detail v-if="mod" :mod="mod" />
    </registry-shell>
</template>
