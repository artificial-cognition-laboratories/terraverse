<script setup lang="ts">
import type { Span } from "./markdown.ts"

/** One run of inline markdown. Links show where they go but don't navigate: the page is the game's. */
defineProps<{ spans: readonly Span[] }>()
</script>

<template>
    <template v-for="(s, i) in spans" :key="i">
        <code v-if="s.code">{{ s.text }}</code>
        <img v-else-if="s.image" class="inline-image" :src="s.image" :alt="s.text">
        <span v-else-if="s.href" class="link" :title="s.href">{{ s.text }}</span>
        <strong v-else-if="s.bold">{{ s.text }}</strong>
        <em v-else-if="s.em">{{ s.text }}</em>
        <template v-else>{{ s.text }}</template>
    </template>
</template>
