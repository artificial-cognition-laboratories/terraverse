<script setup lang="ts">
import { parseMarkdown } from "@nuxtjs/mdc/runtime"
import { markdownHighlighter } from "~/lib/highlight"

/**
 * A mod's README, set as plain text on the sky under the header: no card.
 * TEMP(2026-11-21): the sample readmes are ours. Before real ones are shown, raw
 * HTML in them must be stripped (no script, style or iframe from a stranger).
 */
const props = defineProps<{ name: string; markdown: string }>()
// The header above already says the mod's name: a readme's opening title would say it twice
const body = computed(() => props.markdown.replace(/^\s*# [^\n]*\n+/, ""))
const { data: ast } = await useAsyncData(`readme:${props.name}`, () => parseMarkdown(body.value, { highlight: { highlighter: markdownHighlighter } }), { watch: [body] })
</script>

<template>
    <article class="readme">
        <MDCRenderer v-if="ast" :body="ast.body" :data="ast.data" />
    </article>
</template>

<style scoped lang="scss">
.readme {
    @include on-sky;
    font-family: $font-sans;
    font-size: 0.95rem;
    line-height: 1.7;
    color: $ink-dim;

    :deep(> div > :first-child) {
        margin-top: 0;
    }

    :deep(h1),
    :deep(h2),
    :deep(h3) {
        margin: 2rem 0 0.6rem;
        font-weight: 600;
        color: $ink;
    }

    :deep(h1) {
        font-size: 1.3rem;
    }

    :deep(h2) {
        font-size: 1.05rem;
    }

    :deep(h3) {
        font-size: 0.95rem;
    }

    :deep(p) {
        margin: 0 0 1rem;
    }

    :deep(a) {
        color: $primary-ink;
    }

    // headings carry their own anchor: they read as headings, not links
    :deep(:is(h1, h2, h3) a) {
        color: inherit;
        text-decoration: none;
    }

    :deep(code) {
        font-family: $font-mono;
        font-size: 0.85em;
        color: $ink;
    }

    // code sits on a faint well, not a box: no edge to compete with the text
    :deep(pre) {
        margin: 0 0 1rem;
        padding: 0.8rem 1rem;
        overflow-x: auto;
        background: rgba($night-0, 0.8) !important;
        backdrop-filter: blur(8px);
        border: none;
    }

    :deep(blockquote) {
        margin: 1.5rem 0 0;
        padding: 0.1rem 1rem;
        color: $ink-faint;
        border-left: 2px solid rgba($primary-ink, 0.55);

        p {
            margin: 0;
        }
    }
}
</style>
