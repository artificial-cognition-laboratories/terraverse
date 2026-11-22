<script setup lang="ts">
import { computed, ref } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { Palette, type PaletteItem } from "../../kit/index.ts"
import { useStore } from "../store.ts"
import { commandLine } from "./commands.ts"
import { viewOf, type Context, type Entry, type View } from "./lists.ts"

/**
 * Chat and commands, in one line over the game (key T, / for commands, F10 for actions: the
 * engine opens it on that list, store.palette). Plain text is said; `:` runs a command, `@`
 * finds someone, `#` somewhere, `>` an action (lists.ts). What the server answers is chat's,
 * in the feed beside it (game/chat/ChatFeed.vue).
 */
const bridge = useBridge()
const store = useStore()
const query = ref(store.palette)
const players = ref<string[]>([])
void bridge.send("chat.players", {}).then((names) => { players.value = names })

// command lines run lately, the latest first, kept between games
const RECENT_KEY = "palette.recent"
const RECENT_KEPT = 5
const recent = ref<string[]>(JSON.parse(localStorage.getItem(RECENT_KEY) ?? "[]") as string[])
const close = () => void bridge.send("screen.close", {})
const log = ref(false) // the Developer log, in place of the lists

const ctx: Context = {
    store: store,
    bridge: bridge,
    players: players,
    recent: recent,
    go: (next) => { query.value = next },
    run: (text) => {
        recent.value = [text, ...recent.value.filter((line) => line !== text)].slice(0, RECENT_KEPT)
        localStorage.setItem(RECENT_KEY, JSON.stringify(recent.value))
        void bridge.send("chat.send", { text: commandLine.wire(text) })
        close()
    },
    close: close,
    showLog: () => { log.value = true; query.value = "" },
}

const view = computed<View>(() => log.value ? logView(query.value) : viewOf(ctx, query.value))
const items = computed(() => view.value.entries.map((entry) => entry.item))
function pick(item: PaletteItem, how: "enter" | "tab" | "click") {
    view.value.entries.find((entry) => entry.item.id === item.id)?.take(how)
}

// what the engine logged, the latest first, found by what's typed
function logView(text: string): View {
    const want = text.trim().toLowerCase()
    const entries = [...store.log].reverse().filter((line) => line.text.toLowerCase().includes(want)).slice(0, 200)
        .map((line, i): Entry => {
            const at = want ? line.text.toLowerCase().indexOf(want) : -1
            return {
                item: { id: `log:${i}`, label: line.text, marks: at < 0 ? [] : Array.from(want, (_, k) => at + k) },
                take: () => undefined,
            }
        })
    return { entries: entries, placeholder: "Find in the log",
        note: store.log.length ? "Nothing in the log says that." : "Nothing's been logged at chat_log_level or worse." }
}
</script>

<template>
    <Palette v-model:query="query" :items="items" :placeholder="view.placeholder" :note="view.note"
        @pick="pick" @close="close" />
</template>
