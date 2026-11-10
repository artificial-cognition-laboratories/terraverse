<script setup lang="ts">
import { computed } from "vue"

/** Release notes: paragraphs, "- " lists and **bold**, nothing else. Never HTML. */
const props = defineProps<{ text: string }>()

type Run = { text: string; bold: boolean }
type Block = { kind: "p"; runs: Run[] } | { kind: "ul"; items: Run[][] }

function runs(line: string): Run[] {
    return line.split("**").map((text, i) => ({ text, bold: i % 2 === 1 })).filter(run => run.text)
}

const blocks = computed<Block[]>(() => props.text.split(/\n\s*\n/).map(chunk => {
    const lines = chunk.split("\n").map(line => line.trim()).filter(Boolean)
    return lines.every(line => line.startsWith("- "))
        ? { kind: "ul", items: lines.map(line => runs(line.slice(2))) }
        : { kind: "p", runs: runs(lines.join(" ")) }
}))
</script>

<template>
    <div class="notes">
        <template v-for="(block, i) in blocks" :key="i">
            <p v-if="block.kind === 'p'"><template v-for="(run, j) in block.runs" :key="j"><strong v-if="run.bold">{{ run.text }}</strong><template v-else>{{ run.text }}</template></template></p>
            <ul v-else>
                <li v-for="(item, j) in block.items" :key="j"><template v-for="(run, k) in item" :key="k"><strong v-if="run.bold">{{ run.text }}</strong><template v-else>{{ run.text }}</template></template></li>
            </ul>
        </template>
    </div>
</template>

<style scoped>
.notes { display: flex; flex-direction: column; gap: 10px; color: var(--ink-dim); font-size: 13px; line-height: 1.6; user-select: text; }
p, ul { margin: 0; }
ul { padding: 0; list-style: none; display: grid; gap: 4px; }
li { position: relative; padding-left: 18px; }
li::before { content: "»"; position: absolute; left: 0; color: var(--accent); }
strong { color: var(--ink); font-weight: 700; }
</style>
