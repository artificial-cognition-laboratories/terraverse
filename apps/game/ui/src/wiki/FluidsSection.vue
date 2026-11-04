<script setup lang="ts">
import { computed, onMounted, shallowRef } from "vue"
import type { MoveScene } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { ListEmpty, ListGroup, ListItem, WorkbenchColumns } from "../kit/index.ts"
import CourseStage from "./CourseStage.vue"
import { FLUID_DEMOS, type FluidDemo } from "./fluids.ts"
import { useWiki } from "./nav.ts"
import SceneStage from "./SceneStage.vue"

/** What flows and what gives: liquids and powders in the world's matter, loose ground under what presses it. */
const bridge = useBridge()
const wiki = useWiki()
const ground = shallowRef<MoveScene[]>([])
type Item = { id: string; group: string; title: string; note: string; demo?: FluidDemo; move?: MoveScene }
const items = computed((): Item[] => [
    ...FLUID_DEMOS.map((d) => ({ id: d.id, group: d.group, title: d.title, note: d.note, demo: d })),
    ...ground.value.map((m) => ({ id: m.id, group: m.group, title: m.title, note: m.note, move: m })),
])
const groups = computed(() => {
    const out = new Map<string, Item[]>()
    const term = wiki.search.value.trim().toLowerCase()
    for (const i of items.value) {
        if (!`${i.title} ${i.note} ${i.group}`.toLowerCase().includes(term)) continue
        out.set(i.group, [...(out.get(i.group) ?? []), i])
    }
    return [...out.entries()]
})
const chosen = computed(() => items.value.find((i) => i.id === wiki.state.chosen.fluids) ?? items.value[0]!)
onMounted(async () => { ground.value = await bridge.send("moves.list", { scenes: "ground" }) })
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <ListGroup v-for="[group, list] in groups" :key="group" :title="group">
                <ListItem v-for="i in list" :key="i.id" :on="chosen.id === i.id" @click="wiki.go('fluids', i.id)">{{ i.title }}</ListItem>
            </ListGroup>
            <ListEmpty v-if="!groups.length">No matches.</ListEmpty>
        </template>
        <template #content>
            <CourseStage v-if="chosen.move" key="course" scenes="ground" :chosen="chosen.move" />
            <SceneStage v-else-if="chosen.demo" key="flow" :spec="chosen.demo.spec" :speed="chosen.demo.speed">
                <template #facts>
                    <h4>{{ chosen.title }}</h4>
                    <p class="copy">{{ chosen.note }}</p>
                </template>
            </SceneStage>
        </template>
    </WorkbenchColumns>
</template>
