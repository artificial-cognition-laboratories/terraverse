<script setup lang="ts">
import { computed } from "vue"
import { Copier } from "./copy.ts"
import { parseMarkdown } from "./markdown.ts"
import MarkdownSpans from "./MarkdownSpans.vue"

/**
 * A README, drawn from data (markdown.ts), never as HTML, and set as the website sets it
 * (apps/site assets/styles/prose.scss): sans on the night, code blocks as windows that copy.
 * `resolve` says where its images are served.
 */
const props = withDefaults(defineProps<{ source: string; resolve?: (src: string) => string }>(), { resolve: (src: string) => src })
const blocks = computed(() => parseMarkdown(props.source, props.resolve))
const copier = Copier()
const STATE = { idle: "copy", copied: "copied", failed: "can't copy" } as const
</script>

<template>
    <div class="markdown">
        <template v-for="(b, i) in blocks" :key="i">
            <h1 v-if="b.kind === 'heading' && b.level === 1"><MarkdownSpans :spans="b.spans" /></h1>
            <h2 v-else-if="b.kind === 'heading' && b.level === 2"><MarkdownSpans :spans="b.spans" /></h2>
            <h3 v-else-if="b.kind === 'heading' && b.level === 3"><MarkdownSpans :spans="b.spans" /></h3>
            <h4 v-else-if="b.kind === 'heading'"><MarkdownSpans :spans="b.spans" /></h4>
            <p v-else-if="b.kind === 'paragraph'"><MarkdownSpans :spans="b.spans" /></p>
            <blockquote v-else-if="b.kind === 'quote'"><p><MarkdownSpans :spans="b.spans" /></p></blockquote>
            <component :is="b.ordered ? 'ol' : 'ul'" v-else-if="b.kind === 'list'">
                <li v-for="(item, j) in b.items" :key="j"><MarkdownSpans :spans="item" /></li>
            </component>
            <figure v-else-if="b.kind === 'code'" class="window">
                <figcaption>
                    <span class="lang">{{ b.lang }}</span>
                    <span class="dots" aria-hidden="true"><i /><i /><i /></span>
                    <button type="button" class="copy" @click="copier.copy(b.text)">{{ STATE[copier.state.value] }}</button>
                </figcaption>
                <pre><code>{{ b.text }}</code></pre>
            </figure>
            <table v-else-if="b.kind === 'table'">
                <thead><tr><th v-for="(c, j) in b.head" :key="j"><MarkdownSpans :spans="c" /></th></tr></thead>
                <tbody><tr v-for="(row, j) in b.rows" :key="j"><td v-for="(c, k) in row" :key="k"><MarkdownSpans :spans="c" /></td></tr></tbody>
            </table>
            <figure v-else-if="b.kind === 'image'" class="image"><img :src="b.src" :alt="b.alt"><figcaption v-if="b.alt">{{ b.alt }}</figcaption></figure>
            <hr v-else-if="b.kind === 'rule'">
        </template>
    </div>
</template>

<style scoped>
.markdown { min-width: 0; overflow-wrap: break-word; color: var(--ink-dim); font: 0.95rem/1.75 var(--font-sans); user-select: text; }
.markdown > :first-child { margin-top: 0; padding-top: 0; border-top: 0; }
h1, h2, h3, h4 { margin: 2.5rem 0 0.75rem; font-weight: 600; line-height: 1.25; letter-spacing: -0.01em; color: var(--ink); }
h1 { font-size: 2.1rem; letter-spacing: -0.02em; }
h2 { padding-top: 2rem; font-size: 1.35rem; border-top: 1px solid var(--glass-edge); }
h3 { font-size: 1.05rem; }
h4 { font-size: 0.95rem; color: var(--ink-dim); }
p { margin: 0 0 1.1rem; }
ul, ol { margin: 0 0 1.25rem; padding-left: 1.4rem; }
li { margin: 0.35rem 0; padding-left: 0.25rem; }
li::marker { color: var(--accent-hi); }
:deep(strong) { font-weight: 600; color: var(--ink); }
:deep(.link) { color: var(--accent-hi); border-bottom: 1px solid rgb(99 163 233 / 0.35); }
:not(pre) > :deep(code) { padding: 0.1em 0.35em; color: var(--ink); background: rgb(231 237 244 / 0.07); font: 0.84em var(--font); }
blockquote { margin: 1.5rem 0; padding: 0.1rem 1rem; border-left: 2px solid var(--edge-lit-hi); }
blockquote p { margin: 0.5rem 0; }
table { width: 100%; margin: 1.5rem 0; border-collapse: collapse; font-size: 0.85rem; }
th { padding: 0.5rem 1rem 0.5rem 0; border-bottom: 1px solid var(--glass-edge); color: var(--ink-faint); font: 400 0.6rem var(--font-display); letter-spacing: 0.04em; text-align: left; text-transform: uppercase; }
td { padding: 0.6rem 1rem 0.6rem 0; border-bottom: 1px solid rgb(231 237 244 / 0.05); vertical-align: top; }
tr:last-child td { border-bottom: 0; }
hr { margin: 2.5rem 0; border: 0; border-top: 1px solid var(--glass-edge); }
.image { margin: 0 0 1.1rem; }
.image img { max-width: 100%; height: auto; border: 1px solid var(--glass-edge); border-radius: var(--radius); image-rendering: auto; }
.image figcaption { color: var(--ink-faint); font-size: 0.75rem; }
:deep(.inline-image) { height: 1.4em; vertical-align: middle; }

/* a code block: the website's window (home/window.vue, content/ProsePre.vue): its language on the
   left, dots on the right that give way to "copy" when it's pointed at */
.window { margin: 1.75rem 0; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: var(--glass); backdrop-filter: blur(8px); }
.window figcaption { position: relative; display: flex; align-items: center; gap: 1rem; min-height: 2.8rem; padding: 0 1rem; }
.lang { flex: 1; color: var(--ink-faint); font: 0.72rem var(--font); }
.dots { display: flex; gap: 0.4rem; }
.dots i { width: 0.6rem; height: 0.6rem; border-radius: 50%; background: var(--night-4); }
.copy { position: absolute; top: 50%; right: 1rem; padding: 0; border: 0; background: none; color: var(--ink-faint); font: 0.68rem var(--font); opacity: 0; transform: translateY(-50%); }
.copy:hover { color: var(--ink); }
.window:hover .copy { opacity: 1; }
.window:hover .dots { opacity: 0; }
pre { margin: 0; padding: 0 1.25rem 1.1rem; overflow-x: auto; color: var(--ink); font: 0.8rem/1.65 var(--font); tab-size: 4; }
pre code { font: inherit; }
</style>
