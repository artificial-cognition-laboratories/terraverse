<script setup lang="ts">
import { parseMarkdown } from "@nuxtjs/mdc/runtime"
import { markdownHighlighter } from "~/lib/highlight"
import ProsePre from "~/components/content/ProsePre.vue"

/**
 * A mod's README, set as the docs are (`.prose`): plain text on the sky under the header.
 * TEMP(2026-11-21): the sample readmes are ours. Before real ones are shown, raw
 * HTML in them must be stripped (no script, style or iframe from a stranger).
 */
const props = defineProps<{ name: string; markdown: string }>()
// The header above already says the mod's name: a readme's opening title would say it twice
const body = computed(() => props.markdown.replace(/^\s*# [^\n]*\n+/, ""))
// The docs' code windows: MDCRenderer doesn't find the override by name as ContentRenderer does
const PROSE = { pre: ProsePre }
const { data: ast } = await useAsyncData(`readme:${props.name}`, () => parseMarkdown(body.value, { highlight: { highlighter: markdownHighlighter } }), { watch: [body] })
</script>

<template>
    <article class="readme prose">
        <MDCRenderer v-if="ast" :body="ast.body" :data="ast.data" :components="PROSE" />
    </article>
</template>

<style scoped lang="scss">
// Under the header the readme's first heading starts the page, so it needs no gap above
.readme :deep(> div > :first-child) {
    margin-top: 0;
    padding-top: 0;
    border-top: none;
}
</style>
