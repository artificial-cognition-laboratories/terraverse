<script setup lang="ts">
import { defineAsyncComponent, ref } from "vue"
import Corner from "../account/Corner.vue"
import { useBridge } from "../bridge/use.ts"
import PixelIcon from "../game/PixelIcon.vue"
import { IconTile } from "../kit/index.ts"
import Attributions from "./Attributions.vue"
import CreateWorld from "./create/CreateWorld.vue"
import Home from "./Home.vue"
import Community from "./Community.vue"
import Settings from "./Settings.vue"
import type { Screen } from "./screens.ts"
import type { SystemWire, UpdateCheck } from "../bridge"
import WorldSelect from "./WorldSelect.vue"
import { useMenuMusic } from "./music.ts"

// the wiki brings three.js: loaded when first opened
const WikiScreen = defineAsyncComponent(() => import("../wiki/WikiScreen.vue"))

const props = withDefaults(defineProps<{ error: string | null; playMusic?: boolean }>(), { playMusic: true })
defineEmits<{ dismiss: [] }>()
useMenuMusic(props.playMusic)

const screen = ref<Screen>("home")
const seedCode = ref<string | null>(null)
const bridge = useBridge()
// null until the engine answers; a failed check still names the running version
const update = ref<UpdateCheck | null>(null)
bridge.send("update.check", {}).then(answer => { update.value = answer })
const REPO = "https://github.com/artificial-cognition-laboratories/terraverse"

// the system a new world's draft starts from: the engine's, asked once
const home = ref<SystemWire | null>(null)
const homeProblem = ref<string | null>(null)
async function create() {
    if (!home.value) {
        try {
            home.value = await bridge.send("system.home", {})
        } catch (error) {
            // shown on the menu: the create screen can't open without it
            homeProblem.value = error instanceof Error ? error.message : String(error)
            console.error("system.home", error)
            return
        }
    }
    screen.value = "create"
}
function go(next: Screen) {
    if (next === "create") {
        seedCode.value = null
        return void create()
    }
    screen.value = next
}
function createFromCode(code: string) {
    seedCode.value = code
    void create()
}

// a 9×9 pictogram for the workshop row: an open book
const WIKI_ICON = {
    rows: [".........", "BBBB.BBBB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BPLB.BLPB", "BPPB.BPPB", "BBBBBBBBB", "....B...."],
    palette: { B: "#c9d4dc", P: "#f0ead8", L: "#8a8478" },
} as const
const CREDITS_ICON = {
    rows: ["..CCCCC..", ".C.....C.", "C..CCC..C", "C.C...C.C", "C.C.C.C.C", "C.C...C.C", "C..CCC..C", ".C.....C.", "..CCCCC.."],
    palette: { C: "#c9d4dc" },
} as const
</script>

<template>
    <main class="menu">
        <div v-if="homeProblem" class="error" role="alert">
            <span><strong>Can't start a world:</strong> {{ homeProblem }}</span>
            <button @click="homeProblem = null">✕</button>
        </div>
        <div v-if="error" class="error" role="alert">
            <span><strong>Session ended:</strong> {{ error }}</span>
            <button @click="$emit('dismiss')">✕</button>
        </div>

        <Transition name="screen" mode="out-in">
            <Home v-if="screen === 'home'" @go="go" />
            <WorldSelect v-else-if="screen === 'worlds'" @go="go" />
            <CreateWorld v-else-if="screen === 'create' && home" :initial-code="seedCode" :home="home" @go="go" />
            <Settings v-else-if="screen === 'settings'" @go="go" />
            <Community v-else-if="screen === 'community'" @go="go" @create-code="createFromCode" />
            <Attributions v-else-if="screen === 'attributions'" @go="go" />
            <WikiScreen v-else-if="screen === 'wiki'" place="menu" @close="screen = 'home'" />
        </Transition>

        <!-- the home screen's corners: outside the screen transition, whose
             transform would carry anything pinned to the window along with it -->
        <Transition name="corners">
            <div v-if="screen === 'home'" class="corners">
                <Corner />
                <!-- the workshop: places to see the engine's building blocks and mechanics -->
                <nav class="tools" aria-label="Workshop">
                    <IconTile label="Wiki — what everything is made of, and how it behaves" side="left" @click="screen = 'wiki'">
                        <PixelIcon :rows="WIKI_ICON.rows" :palette="WIKI_ICON.palette" style="--u: 3px" />
                    </IconTile>
                    <IconTile label="Terraverse on GitHub" side="left" @click="bridge.send('open', { url: REPO })">
                        <svg viewBox="0 0 16 16" width="23" height="23" aria-hidden="true">
                            <path fill="currentColor" d="M8 1a7 7 0 0 0-2.21 13.64c.35.06.48-.15.48-.34v-1.19c-1.96.42-2.37-.83-2.37-.83-.32-.81-.78-1.03-.78-1.03-.64-.44.05-.43.05-.43.71.05 1.08.73 1.08.73.63 1.07 1.65.76 2.05.58.06-.46.25-.76.45-.93-1.56-.18-3.2-.78-3.2-3.48 0-.77.27-1.4.72-1.9-.07-.18-.31-.9.07-1.87 0 0 .59-.19 1.93.73A6.7 6.7 0 0 1 8 4.45c.59 0 1.18.08 1.73.23 1.34-.92 1.93-.73 1.93-.73.38.97.14 1.69.07 1.87.45.5.72 1.13.72 1.9 0 2.71-1.64 3.3-3.2 3.47.25.22.48.64.48 1.29v1.82c0 .19.13.4.48.34A7 7 0 0 0 8 1Z" />
                        </svg>
                    </IconTile>
                    <IconTile label="Attributions" side="left" @click="screen = 'attributions'">
                        <PixelIcon :rows="CREDITS_ICON.rows" :palette="CREDITS_ICON.palette" style="--u: 3px" />
                    </IconTile>
                </nav>
                <footer>
                    <span v-if="update">{{ update.current }} · </span>built on luanti
                    <p v-if="update?.available" class="update"><span>»</span> v{{ update.latest }} is out. Run <code>terraverse update</code></p>
                </footer>
            </div>
        </Transition>
    </main>
</template>

<style scoped>
.menu {
    position: relative;
    height: 100%;
    display: grid;
    place-items: center;
    overflow: hidden;
    background: transparent;
}
.menu:has(.home)::before {
    /* behind Home's title only: anywhere else it would take the mouse from
       the planet (the shell gives the page any pixel it draws at all) */
    content: "";
    position: absolute;
    inset: 0;
    pointer-events: none;
    background:
        radial-gradient(ellipse 27% 48% at 50% 52%, rgb(3 8 15 / 0.68), transparent 100%),
        linear-gradient(90deg, rgb(3 8 15 / 0.18), transparent 35%, transparent 70%, rgb(3 8 15 / 0.08));
}
.menu > :deep(section), .menu > :deep(.home) { position: relative; z-index: 1; }

.error {
    position: absolute;
    top: 20px;
    z-index: 2;
    display: flex;
    gap: 16px;
    align-items: center;
    padding: 10px 14px;
    border: var(--b) solid var(--edge);
    background: var(--danger-lo);
    box-shadow: var(--raised), 6px 6px 0 rgb(0 0 0 / 0.35);
}
.error button { border: 0; background: none; padding: 0 4px; }

.corners { position: absolute; inset: 0; z-index: 1; pointer-events: none; }
.corners > * { pointer-events: auto; }
.tools { position: absolute; right: 20px; bottom: 20px; display: flex; gap: 10px; }
footer .update { margin: 4px 0 0; color: var(--ink); }
footer .update code { font-family: "JetBrains Mono", monospace; text-transform: none; }
footer .update span { color: rgb(0 148 210); }
footer {
    position: absolute;
    left: 20px;
    bottom: 14px;
    font: 11px var(--font-display);
    color: var(--ink-faint);
    text-transform: uppercase;
}
.corners-enter-active, .corners-leave-active { transition: opacity 140ms steps(4); }
.corners-enter-from, .corners-leave-to { opacity: 0; }

/* opacity only: a transform on a screen moves what it pins to the window (Dock, its
   buttons) to its own box for as long as the transition runs, and back with a jump */
.screen-enter-active, .screen-leave-active { transition: opacity 140ms steps(4); }
.screen-enter-from, .screen-leave-to { opacity: 0; }
</style>
