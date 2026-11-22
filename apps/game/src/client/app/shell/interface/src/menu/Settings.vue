<script setup lang="ts">
import { ref } from "vue"
import { onEscape } from "../kit/index.ts"
import SettingsPanel from "../settings/SettingsPanel.vue"
import type { Screen } from "./screens.ts"

const emit = defineEmits<{ go: [screen: Screen] }>()
onEscape(() => emit("go", "home"))
const query = ref("")
</script>

<template>
    <SettingsPanel v-model:query="query" close-label="Done" @close="$emit('go', 'home')" />
</template>
