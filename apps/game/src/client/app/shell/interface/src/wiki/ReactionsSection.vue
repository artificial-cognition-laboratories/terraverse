<script setup lang="ts">
import { ListEmpty, ListGroup, ListItem, WorkbenchColumns } from "../kit/index.ts"
import { computed } from "vue"
import { useTable, useWiki } from "./nav.ts"
import { reactionDemo } from "./demos.ts"
import SceneStage from "./SceneStage.vue"

/**
 * Reactions: every reaction the engine runs, each shown happening in a
 * sandbox built from its own equation and numbers. What goes in and what
 * comes out link to Matter.
 */
const wiki = useWiki()
const table = useTable()
const matches = (text: string) => text.toLowerCase().includes(wiki.search.value.trim().toLowerCase())

const groups = computed(() => [
    { label: "Give off heat", items: table.reactions.filter((r) => !r.energy && r.heat > 0 && matches(`${r.name} ${r.note} ${r.id}`)) },
    { label: "Take in heat", items: table.reactions.filter((r) => !r.energy && r.heat <= 0 && matches(`${r.name} ${r.note} ${r.id}`)) },
    { label: "Run on current or light", items: table.reactions.filter((r) => r.energy && matches(`${r.name} ${r.note} ${r.id}`)) },
])
const chosen = computed(() => table.reactions.find((r) => r.id === wiki.state.chosen.reactions) ?? table.reactions[0] ?? null)
const demo = computed(() => (chosen.value ? reactionDemo(chosen.value, table.byId) : null))
const formula = (id: string) => table.byId.get(id)?.formula ?? id
const name = (id: string) => table.byId.get(id)?.name ?? id
const terms = (side: Record<string, number>) => Object.entries(side).map(([id, n]) => ({ id: id, n: n }))
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <template v-for="g in groups" :key="g.label">
                <ListGroup v-if="g.items.length" :title="g.label">
                    <ListItem v-for="r in g.items" :key="r.id" :on="chosen?.id === r.id" @click="wiki.go('reactions', r.id)">
                        {{ r.name }}
                    </ListItem>
                </ListGroup>
            </template>
            <ListEmpty v-if="wiki.search.value && !groups.some((g) => g.items.length)">No matches.</ListEmpty>
        </template>

        <template #content>
            <template v-if="chosen && demo">
                <SceneStage :spec="demo.runnable ? demo.spec : null" :speed="1">
                <template #top><p class="equation">
                    <template v-for="(t, k) in terms(chosen.reactants)" :key="'r' + t.id">
                        <span v-if="k" class="op">+</span>
                        <span v-if="t.n !== 1" class="n">{{ t.n }}</span><button class="link term" @click="wiki.go('matter', t.id)">{{ formula(t.id) }}</button>
                    </template>
                    <span class="op">→</span>
                    <template v-for="(t, k) in terms(chosen.products)" :key="'p' + t.id">
                        <span v-if="k" class="op">+</span>
                        <span v-if="t.n !== 1" class="n">{{ t.n }}</span><button class="link term" @click="wiki.go('matter', t.id)">{{ formula(t.id) }}</button>
                    </template>
                </p>
                <p v-if="!demo.runnable" class="unrunnable">{{ demo.reason }}</p></template>
                <template #facts>
                <h4>{{ chosen.name }}</h4>
                <p class="copy">{{ chosen.note }}</p>
                <dl>
                    <div><dt>Heat</dt><dd>{{ chosen.heat > 0 ? "gives off" : "takes in" }} {{ Math.round(Math.abs(chosen.heat) * 10) / 10 }} kJ as written</dd></div>
                    <div v-if="chosen.energy"><dt>Driven by</dt><dd>{{ chosen.energy }}</dd></div>
                    <template v-else>
                        <div><dt>Activation energy</dt><dd>{{ chosen.activation }} kJ/mol</dd></div>
                        <div v-if="demo?.runnable"><dt>Runs visibly from</dt><dd>about {{ Math.round(demo.temperature) }} K</dd></div>
                    </template>
                    <div v-for="q in chosen.requires ?? []" :key="q.substance"><dt>Needs</dt><dd>{{ name(q.substance) }} {{ q.state }}</dd></div>
                </dl>
                <p v-if="demo?.runnable" class="dim">{{ demo.note }}</p>
                <h5>In</h5>
                <div class="links"><button v-for="t in terms(chosen.reactants)" :key="t.id" class="link" @click="wiki.go('matter', t.id)">{{ name(t.id) }}</button></div>
                <h5>Out</h5>
                <div class="links"><button v-for="t in terms(chosen.products)" :key="t.id" class="link" @click="wiki.go('matter', t.id)">{{ name(t.id) }}</button></div>
                </template>
                </SceneStage>
            </template>
        </template>
    </WorkbenchColumns>
</template>

<style scoped>
.equation { display: flex; flex-wrap: wrap; align-items: baseline; gap: 6px; margin: 0; font: 16px var(--font-display); }
.term { font: 16px var(--font-display); }
.n { color: var(--ink-dim); }
.op { color: var(--ink-faint); padding: 0 4px; }
.unrunnable { margin: 8px 0 0; max-width: 380px; color: var(--ink-dim); font-size: 12px; line-height: 1.5; }
</style>
