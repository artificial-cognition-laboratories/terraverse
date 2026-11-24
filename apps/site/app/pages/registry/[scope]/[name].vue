<script setup lang="ts">
import { useRegistry } from "~/registry"

definePageMeta({ layout: "landing" })

// The address is the mod's name: /registry/@scope/name
const route = useRoute()
const name = computed(() => `${route.params.scope}/${route.params.name}`)
const reg = useRegistry()
const { data: mod } = await useLive(() => `registry:mod:${name.value}`, () => reg.mod(name.value))
missing(() => !String(route.params.scope).startsWith("@") || mod.value === null, () => `There's no ${name.value} in the registry.`)
useHead({ title: () => mod.value?.title ?? "Registry" })
useSeoMeta({ description: () => mod.value?.description ?? "" })
</script>

<template>
    <registry-shell>
        <registry-detail v-if="mod" :mod="mod" />
    </registry-shell>
</template>
