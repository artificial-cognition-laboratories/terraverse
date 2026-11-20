<script setup lang="ts">
import { ref } from "vue"
import { Button, ListEmpty, ListGroup, ListItem, Notice, Panel, WorkbenchPanel } from "../../kit/index.ts"
import Specimen from "../Specimen.vue"

const group = ref("mods")
const query = ref("")
const demoItems = ["Flight", "Matter", "World"]
const picked = ref("Matter")
const groups = [
    { id: "mods", label: "Mods", rows: ["..AAA..", ".AABAA.", "AAABAAA", ".AABAA.", "..AAA.."], palette: { A: "#c9d4dc", B: "#40b8ec" } },
    { id: "worlds", label: "Worlds", rows: ["..AAA..", ".AABAA.", "AABBBAA", ".AABAA.", "..AAA.."], palette: { A: "#c9d4dc", B: "#4a9b64" } },
]
</script>

<template>
    <div class="grid">
        <Specimen name="Panel" note="title, actions, body, footer">
            <Panel title="Worlds" class="demo">
                <template #actions><Button size="sm">Back</Button></template>
                <p>Panel body. Scrolls when it overflows.</p>
                <template #footer><Button variant="primary">Play »</Button></template>
            </Panel>
        </Specimen>
        <Specimen name="Panel" note="body only">
            <Panel class="demo"><Notice>A bare panel with no title bar.</Notice></Panel>
        </Specimen>
        <Specimen name="WorkbenchPanel" note="shared header, search, rail, list, content, and optional facts" wide>
            <WorkbenchPanel v-model:group="group" v-model:query="query" class="workbench-demo"
                title="Community" :section="group === 'mods' ? 'Mods' : 'Worlds'"
                :groups="groups" rail-label="Example groups" search-label="Search library">
                <template #actions><Button size="sm">Back</Button></template>
                <template #list>
                    <ListGroup title="Library">
                        <ListItem v-for="item in demoItems.filter((name) => name.toLowerCase().includes(query.toLowerCase()))"
                            :key="item" :on="picked === item" @click="picked = item">
                            <template #lead>{{ item.slice(0, 2) }}</template>{{ item }}<template #sub>Built-in</template>
                        </ListItem>
                        <ListItem faint>Not seen yet</ListItem>
                        <ListItem>With a status<template #trail>Active</template></ListItem>
                    </ListGroup>
                    <ListEmpty v-if="!demoItems.some((name) => name.toLowerCase().includes(query.toLowerCase()))">No matches.</ListEmpty>
                </template>
                <template #content><p class="copy">The view: what the chosen entry is, shown.</p></template>
                <template #facts>
                    <h4>{{ picked }}</h4>
                    <p class="dim">Built-in · example</p>
                    <p class="copy">The facts column: what it is, in numbers and links.</p>
                    <dl><div><dt>Version</dt><dd>1.0</dd></div><div><dt>Size</dt><dd>12 kB</dd></div></dl>
                    <h5>See also</h5>
                    <div class="links"><button class="link">Matter</button><button class="link">World</button></div>
                </template>
            </WorkbenchPanel>
        </Specimen>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(420px, 1fr)); gap: 28px; }
.demo { width: 100%; height: 260px; }
p { margin: 0; color: var(--ink-dim); }
.grid .workbench-demo { width: 100%; height: 520px; }
h3 { margin: 0 0 12px; font: 18px var(--font-display); text-transform: uppercase; }
</style>
