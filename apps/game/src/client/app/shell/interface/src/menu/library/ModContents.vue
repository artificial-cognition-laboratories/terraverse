<script setup lang="ts">
import { watch } from "vue"
import type { CreativeItem } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"
import { Spinner } from "../../kit/index.ts"
import LibraryBrowser from "../../wiki/LibraryBrowser.vue"
import { Request } from "./request.ts"

/**
 * A mod's blocks and items: the list at the left, the one picked turning at the right, as
 * the game draws them. terra loads the mod with no world to draw them (bridge/local_mods.h).
 */
const props = defineProps<{ path: string; /** Bumped to read the mod again (after a save). */ reload: number }>()
const emit = defineEmits<{ count: [n: number] }>()
const bridge = useBridge()
const contents = Request<string, CreativeItem[]>((path) => bridge.send("mods.contents", { path: path }))
watch(() => [props.path, props.reload] as const, ([path]) => void contents.run(path), { immediate: true })
watch(() => contents.data.value, (items) => { if (items) emit("count", items.length) })
</script>

<template>
    <div class="mod-contents">
        <div v-if="!contents.data.value && !contents.problem.value" class="loading"><Spinner /> Reading what's in it…</div>
        <LibraryBrowser v-else :catalog="contents.data.value ?? []" :problem="contents.problem.value" />
    </div>
</template>

<style scoped>
.mod-contents { height: 560px; border: var(--b) solid var(--edge); background: var(--stone-0); }
.loading { display: flex; gap: 8px; align-items: center; justify-content: center; height: 100%; color: var(--ink-faint); font-size: 12px; }
</style>
