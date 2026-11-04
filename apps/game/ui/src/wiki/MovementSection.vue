<script setup lang="ts">
import { computed, onMounted, shallowRef } from "vue"
import type { MoveScene, SceneKind } from "../bridge/index.ts"
import { useBridge } from "../bridge/use.ts"
import { ListEmpty, ListGroup, ListItem, WorkbenchColumns } from "../kit/index.ts"
import CourseStage from "./CourseStage.vue"
import { useWiki } from "./nav.ts"

/** A list of things that move, and the chosen one playing: a body's moves, a machine's driving. */
const props = defineProps<{ kind: "movement" | "vehicles" }>()
const scenes: SceneKind = props.kind
const bridge = useBridge()
const wiki = useWiki()
const moves = shallowRef<MoveScene[]>([])
const groups = computed(() => {
    const out = new Map<string, MoveScene[]>()
    const term = wiki.search.value.trim().toLowerCase()
    for (const m of moves.value) {
        if (!`${m.title} ${m.note} ${m.group} ${m.keys}`.toLowerCase().includes(term)) continue
        out.set(m.group, [...(out.get(m.group) ?? []), m])
    }
    return [...out.entries()]
})
const chosen = computed(() => moves.value.find((m) => m.id === wiki.state.chosen[props.kind]) ?? moves.value[0] ?? null)
onMounted(async () => { moves.value = await bridge.send("moves.list", { scenes: scenes }) })
</script>

<template>
    <WorkbenchColumns open>
        <template #list>
            <ListGroup v-for="[group, list] in groups" :key="group" :title="group">
                <ListItem v-for="m in list" :key="m.id" :on="chosen?.id === m.id" @click="wiki.go(kind, m.id)">{{ m.title }}</ListItem>
            </ListGroup>
            <ListEmpty v-if="!groups.length">No matches.</ListEmpty>
        </template>
        <template #content>
            <CourseStage v-if="chosen" :scenes="scenes" :chosen="chosen" />
        </template>
    </WorkbenchColumns>
</template>
