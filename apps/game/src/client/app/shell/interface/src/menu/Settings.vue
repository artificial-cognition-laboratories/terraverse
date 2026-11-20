<script setup lang="ts">
import { ref } from "vue"
import SettingsPanel from "../settings/SettingsPanel.vue"
import type { Screen } from "./screens.ts"

defineEmits<{ go: [screen: Screen] }>()
const query = ref("")
</script>

<template>
    <SettingsPanel v-model:query="query" close-label="Done" @close="$emit('go', 'home')" />
</template>
