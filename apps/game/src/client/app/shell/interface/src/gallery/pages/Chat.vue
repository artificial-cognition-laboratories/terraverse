<script setup lang="ts">
import { onMounted } from "vue"
import ChatFeed from "../../game/chat/ChatFeed.vue"
import PaletteScreen from "../../game/palette/PaletteScreen.vue"
import { useStore } from "../../game/store.ts"
import { Button } from "../../kit/index.ts"
import { useMock } from "../mock.ts"

/**
 * Chat and the palette over a game: the commands a player might be given, a few lines said.
 * `?prefix=:` (or @, #, >) opens it on that list, as the keys do.
 */
const bridge = useMock()
const store = useStore()
const prefix = new URLSearchParams(location.search).get("prefix") ?? ""
onMounted(() => {
    bridge.emit({ kind: "mode", mode: "game" })
    bridge.emit({ kind: "ui", key: "wiki", value: true })
    bridge.emit({ kind: "ui", key: "body", value: { id: "home", name: "Terra", home: true } })
    bridge.emit({ kind: "ui", key: "commands", value: [
        { name: "travel", usage: "<body> [<latitude> <longitude>]", description: "Go to another body of the world (degrees; its equator when no place is given)" },
        { name: "river", usage: "", description: "What the river you stand in is doing: its flow, against its usual, its level and speed" },
        { name: "msg", usage: "<to> <text>", description: "Say something to one player", args: [{ name: "to", type: "player", optional: false }, { name: "text", type: "string", optional: false }] },
        { name: "players", usage: "", description: "Who's here", args: [] },
        { name: "kick", usage: "<who> [<reason>]", description: "Disconnect a player, saying why. They can come back.", args: [{ name: "who", type: "player", optional: false }, { name: "reason", type: "string", optional: true }] },
        { name: "rivers", usage: "storm|drought [<hours> [<km>]] | clear", description: "Bring a storm or a drought over the rivers round you" },
    ] })
    store.worlds = [
        { name: "Pine Ridge", path: "/worlds/pine", code: "TF9.pine" },
        { name: "Saltwater", path: "/worlds/salt", code: "TF9.salt" },
    ] as typeof store.worlds
    for (const line of [
        { from: "wren", text: "found copper by the river, west of spawn", tone: "said" },
        { from: null, text: "ash joined the world", tone: "server" },
        { from: "ash", text: "on my way", tone: "said" },
    ] as const) bridge.emit({ kind: "chat", line: line })
    bridge.emit({ kind: "log", level: "error", text: "2026-11-22 21:23:25: ERROR[Main]: Game: the server sent the legacy death screen, which nothing shows" })
    bridge.emit({ kind: "screen", screen: "palette", with: { prefix: prefix } })
})
</script>

<template>
    <div class="chat">
        <Button v-if="store.screen !== 'palette'" class="opener" @click="bridge.emit({ kind: 'screen', screen: 'palette', with: { prefix: '' } })">Open the palette (T)</Button>
        <ChatFeed />
        <PaletteScreen v-if="store.screen === 'palette'" :key="store.palette" />
    </div>
</template>

<style scoped>
.chat { position: fixed; inset: 0; z-index: 5; background: radial-gradient(ellipse at 60% 40%, #1d2b1f, #0b1210); }
.opener { position: absolute; top: 2rem; left: 50%; transform: translateX(-50%); }
</style>
