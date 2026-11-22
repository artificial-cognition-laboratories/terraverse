<script setup lang="ts">
import { Sky } from "../../kit/index.ts"
import WorldSelect from "../../menu/WorldSelect.vue"
import { cannedEntering } from "../mock.ts"

/**
 * Play over the night (the game draws the chosen world's planet there): the dock and the stage.
 * `?entering` shows it with a world on its way, part through joining.
 */
const entering = new URLSearchParams(location.search).has("entering") ? cannedEntering(0.8) : null
</script>

<template>
    <Sky class="play"><WorldSelect :entering="entering" :error="null" /></Sky>
</template>

<style scoped>
.play { position: fixed; inset: 0; z-index: 5; }
</style>
