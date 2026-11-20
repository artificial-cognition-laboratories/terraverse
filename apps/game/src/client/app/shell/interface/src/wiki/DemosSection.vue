<script setup lang="ts">
import { computed } from "vue"
import type { Section } from "./nav.ts"
import { ListEmpty, ListGroup, ListItem, WorkbenchColumns } from "../kit/index.ts"
import type { SandboxSpec } from "../bridge/index.ts"
import { type LabDemo } from "./demos.ts"
import { useWiki } from "./nav.ts"
import SceneStage from "./SceneStage.vue"

/** How matter behaves: one thing each, in a few blocks, played from where it starts. */
const props = defineProps<{ section: Section; demos: (Omit<LabDemo, "spec"> & { spec: SandboxSpec | null })[] }>()
const wiki = useWiki()
const groups = computed(() => {
    const out = new Map<string, typeof props.demos>()
    const term = wiki.search.value.trim().toLowerCase()
    for (const d of props.demos) {
        if (!`${d.title} ${d.note} ${d.group}`.toLowerCase().includes(term)) continue
        out.set(d.group, [...(out.get(d.group) ?? []), d])
    }
    return [...out.entries()]
})
const chosen = computed(() => props.demos.find((d) => d.id === wiki.state.chosen[props.section]) ?? props.demos[0]!)
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <ListGroup v-for="[group, demos] in groups" :key="group" :title="group">
                <ListItem v-for="d in demos" :key="d.id" :on="chosen.id === d.id" @click="wiki.go(section, d.id)">{{ d.title }}</ListItem>
            </ListGroup>
            <ListEmpty v-if="!groups.length">No matches.</ListEmpty>
        </template>
        <template #content>
            <SceneStage :spec="chosen.spec" :speed="chosen.speed">
                <template #facts>
                    <h4>{{ chosen.title }}</h4>
                    <p class="copy">{{ chosen.note }}</p>
                </template>
            </SceneStage>
        </template>
    </WorkbenchColumns>
</template>
