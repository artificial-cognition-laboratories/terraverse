<script setup lang="ts">
import type { File } from "~/registry"
import { language } from "~/registry/tree"
import { highlightCode } from "~/lib/highlight"
import { format } from "~/lib/format"

/** One file of a version: its path and size over its highlighted text, or a note that it isn't text. */
const props = defineProps<{ file: File; version: string }>()
const { data: html } = await useAsyncData(
    () => `source:${props.version}:${props.file.path}`,
    async () => (props.file.text === null ? null : highlightCode(props.file.text, language(props.file.path))),
    { watch: [() => props.file.path, () => props.version] },
)
const crumbs = computed(() => props.file.path.split("/"))
</script>

<template>
    <home-window class="source">
        <template #bar>
            <span class="path">
                <template v-for="(part, i) in crumbs" :key="i">
                    <span v-if="i" class="slash">/</span><span :class="{ last: i === crumbs.length - 1 }">{{ part }}</span>
                </template>
            </span>
            <span class="size">{{ format.size(file.size) }}</span>
        </template>
        <home-code v-if="html" :html="html" />
        <p v-else-if="file.text === null" class="binary">An image or other binary file: open it in the game, or download the version.</p>
    </home-window>
</template>

<style scoped lang="scss">
.source {
    min-height: 24rem;
}

.path {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    font-family: $font-mono;
    font-size: 0.74rem;
    color: $ink-faint;
    text-overflow: ellipsis;

    .slash {
        margin: 0 0.3rem;
    }

    .last {
        color: $ink;
    }
}

.size {
    font-family: $font-mono;
    font-size: 0.7rem;
    color: $ink-faint;
}

.binary {
    margin: 0;
    padding: 3rem 1.5rem;
    text-align: center;
    font-size: 0.85rem;
    color: $ink-faint;
}
</style>
