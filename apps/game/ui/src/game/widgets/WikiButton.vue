<script setup lang="ts">
import { useBridge } from "../../bridge/use.ts"
import IconButton from "../../kit/IconButton.vue"
import { useStore } from "../store.ts"

/** The wiki, by mouse, beside the world controls: only where the world's rules give it (tf_wiki). */
const bridge = useBridge()
const store = useStore()
</script>

<template>
    <IconButton v-if="store.ui.wiki" class="wiki-button" label="Wiki" @click="bridge.send('screen.open', { name: 'wiki' })">
        <svg viewBox="0 0 16 16" width="25" height="25" shape-rendering="crispEdges" aria-hidden="true">
            <path fill="currentColor" d="M1 2h6v1h1v-1h7v12h-6v1h-2v-1h-6zm1 1v10h5v-9h-1v-1zm7 1v9h5v-10h-4v1z" />
            <path fill="var(--accent-hi)" d="M3 5h3v1h-3zm0 2h3v1h-3zm0 2h3v1h-3zm7-4h3v1h-3zm0 2h3v1h-3zm0 2h3v1h-3z" />
        </svg>
    </IconButton>
</template>

<style scoped>
/* left of the world controls and the account */
.wiki-button { position: fixed; z-index: 6; top: 20px; right: 140px; }
</style>
