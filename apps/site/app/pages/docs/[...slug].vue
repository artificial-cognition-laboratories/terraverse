<template>
    <DocsContent>
        <ContentRenderer v-if="page" :value="page" />
    </DocsContent>
</template>

<script setup lang="ts">
definePageMeta({ layout: "docs" })
const page = await useContentPage("docs")
</script>
