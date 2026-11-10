<template>
    <NuxtLayout>
        <NuxtPage />
    </NuxtLayout>
</template>

<script setup lang="ts">
// A function, which nuxt.config's static head can't hold: the landing page
// has no title of its own and is just "Terraverse"
useHead({ titleTemplate: title => (title ? `${title} / Terraverse` : "Terraverse") })
</script>
