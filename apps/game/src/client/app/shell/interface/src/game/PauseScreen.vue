<script setup lang="ts">
import { ref } from "vue"
import { useBridge } from "../bridge/use.ts"
import { Button } from "../kit/index.ts"
import SettingsPanel from "../settings/SettingsPanel.vue"

/**
 * Esc in-game. The engine has paused singleplayer while this is open; Esc again resumes. The world isn't
 * dimmed and the HUD stays up (the map, the wiki, the profile, the building rails), clickable round the buttons.
 */
const bridge = useBridge()
const view = ref<"menu" | "settings">("menu")
const settingsQuery = ref("")
</script>

<template>
    <div class="pause">
        <nav v-if="view === 'menu'" class="menu" aria-label="Paused">
            <Button variant="primary" @click="bridge.send('screen.close', {})">Resume</Button>
            <Button @click="view = 'settings'">Settings</Button>
            <div class="pair">
                <Button @click="bridge.send('game.leave', {})">Main menu</Button>
                <Button variant="danger" @click="bridge.send('game.quit', {})">Quit</Button>
            </div>
        </nav>
        <SettingsPanel v-else-if="view === 'settings'" v-model:query="settingsQuery" close-label="Back" @close="view = 'menu'" />
    </div>
</template>

<style scoped>
/* over the HUD, but only the buttons take the mouse */
.pause { position: fixed; inset: 0; display: grid; place-items: center; pointer-events: none; }
.pause > * { pointer-events: auto; }
.menu { display: grid; gap: 10px; width: 300px; }
.menu > :deep(.btn), .pair :deep(.btn) { width: 100%; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
</style>
