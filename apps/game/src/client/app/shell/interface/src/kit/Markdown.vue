<script setup lang="ts">
import { computed } from "vue"
import { parseMarkdown } from "./markdown.ts"
import MarkdownSpans from "./MarkdownSpans.vue"

/** A README, drawn from data (markdown.ts), never as HTML. `resolve` says where its images are served. */
const props = withDefaults(defineProps<{ source: string; resolve?: (src: string) => string }>(), { resolve: (src: string) => src })
const blocks = computed(() => parseMarkdown(props.source, props.resolve))
</script>

<template>
    <div class="markdown">
        <template v-for="(b, i) in blocks" :key="i">
            <h2 v-if="b.kind === 'heading' && b.level <= 2"><MarkdownSpans :spans="b.spans" /></h2>
            <h3 v-else-if="b.kind === 'heading'"><MarkdownSpans :spans="b.spans" /></h3>
            <p v-else-if="b.kind === 'paragraph'"><MarkdownSpans :spans="b.spans" /></p>
            <blockquote v-else-if="b.kind === 'quote'"><MarkdownSpans :spans="b.spans" /></blockquote>
            <component :is="b.ordered ? 'ol' : 'ul'" v-else-if="b.kind === 'list'">
                <li v-for="(item, j) in b.items" :key="j"><MarkdownSpans :spans="item" /></li>
            </component>
            <pre v-else-if="b.kind === 'code'"><code>{{ b.text }}</code></pre>
            <figure v-else-if="b.kind === 'image'"><img :src="b.src" :alt="b.alt"><figcaption v-if="b.alt">{{ b.alt }}</figcaption></figure>
            <hr v-else-if="b.kind === 'rule'">
        </template>
    </div>
</template>

<style scoped>
.markdown { display: grid; gap: 10px; max-width: 78ch; min-width: 0; overflow-wrap: anywhere; color: var(--ink-dim); font-size: 12px; line-height: 1.65; user-select: text; }
h2, h3 { margin: 6px 0 0; color: var(--ink); font-family: var(--font-display); text-transform: uppercase; }
h2 { font-size: 14px; padding-bottom: 5px; border-bottom: 1px solid var(--stone-3); }
h3 { font-size: 12px; }
p, ul, ol, blockquote, figure { margin: 0; }
ul, ol { padding-left: 20px; display: grid; gap: 4px; }
blockquote { padding: 8px 12px; border-left: 4px solid var(--stone-4); background: var(--stone-1); }
pre { margin: 0; padding: 12px; overflow-x: auto; border: var(--b) solid var(--edge); background: var(--stone-0); box-shadow: var(--sunk); color: var(--ink); font-size: 11px; line-height: 1.5; }
:deep(code) { color: var(--accent-hi); font-size: 11px; }
pre code { color: inherit; }
:deep(strong) { color: var(--ink); }
:deep(.link) { color: var(--accent-hi); text-decoration: underline; text-decoration-color: var(--accent-lo); }
figure { display: grid; gap: 6px; }
figure img { max-width: 100%; border: var(--b) solid var(--edge); background: var(--stone-1); image-rendering: auto; }
figcaption { color: var(--ink-faint); font-size: 10px; }
:deep(.inline-image) { height: 1.4em; vertical-align: middle; }
hr { width: 100%; border: 0; border-top: 1px solid var(--stone-3); }
</style>
