<script setup lang="ts">
import { useRegistry } from "~/registry"

definePageMeta({ layout: "landing" })

// An owner's page: /registry/@scope
const route = useRoute()
const scope = computed(() => String(route.params.scope))
const reg = useRegistry()
const { data: owner } = await useLive(() => `registry:owner:${scope.value}`, () => reg.owner(scope.value))
missing(() => !scope.value.startsWith("@") || owner.value === null, () => `Nobody called ${scope.value} has published yet.`)
useHead({ title: () => scope.value })
</script>

<template>
    <registry-shell>
        <registry-owner v-if="owner" :owner="owner" />
    </registry-shell>
</template>
