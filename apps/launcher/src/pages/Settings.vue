<script setup lang="ts">
import { computed } from "vue"
import { Button, Notice, Section, Segmented, Toggle } from "@game/kit/index.ts"
import type { Settings } from "../backend/index.ts"
import { bytes, since, useStore } from "../use.ts"

defineEmits<{ report: [] }>()
const store = useStore()
const { state } = store

const ON_LAUNCH: { value: Settings["onLaunch"]; label: string }[] = [
    { value: "hide", label: "Hide" }, { value: "close", label: "Close" }, { value: "stay", label: "Stay open" },
]

const beta = computed({ get: () => state.settings?.beta ?? false, set: value => store.changeSettings({ beta: value }) })
const keepOld = computed({ get: () => state.settings?.keepOld ?? true, set: value => store.changeSettings({ keepOld: value }) })
const onLaunch = computed<Settings["onLaunch"] | null>({
    get: () => state.settings?.onLaunch ?? null,
    set: value => { if (value) void store.changeSettings({ onLaunch: value }) },
})
const fetched = computed(() => state.manifest ? since(state.manifest.fetched) : null)
</script>

<template>
    <div class="settings">
        <div class="column">
            <h2><span class="chev">»</span> Settings</h2>
            <p class="lead">What has to be set before the game starts. Graphics, controls and sound are in the game.</p>

            <Section title="Builds" hint="what Play launches">
                <Toggle v-model="beta" label="Betas count as latest" />
                <p class="dim">On, Play takes the newest build, beta or not. Every version is in the list either way.</p>
                <div class="row">
                    <span class="dim">Versions checked {{ fetched ? `${fetched}${fetched === "just now" ? "" : " ago"}` : "never" }}</span>
                    <Button size="sm" @click="store.refreshManifest()">Check now</Button>
                </div>
                <Notice v-if="state.offline" tone="warn">{{ state.offline }}</Notice>
            </Section>

            <Section title="When the game starts" hint="what the launcher does">
                <Segmented v-model="onLaunch" :options="ON_LAUNCH" />
                <p class="dim">Hidden, it comes back when the game closes, to report a crash if there was one.</p>
            </Section>

            <Section title="Storage">
                <Toggle v-model="keepOld" label="Keep old versions" />
                <p class="dim">Off, a version is removed once you've played a newer one. Worlds are never removed.</p>
                <dl v-if="state.about">
                    <div><dt>Game data</dt><dd class="mono">{{ state.about.dataDir }}</dd></div>
                    <div v-if="store.used.value !== null"><dt>Versions use</dt><dd>{{ bytes(store.used.value) }}</dd></div>
                    <div><dt>Free</dt><dd>{{ bytes(state.about.free) }}</dd></div>
                </dl>
                <div class="row"><Button size="sm" @click="store.openDataFolder()">Open data folder</Button></div>
            </Section>

            <Section title="Help">
                <p class="dim">Something wrong, but no crash? Send the last session's log with a note.</p>
                <div class="row">
                    <Button size="sm" @click="$emit('report')">Report a problem</Button>
                    <Button size="sm" @click="store.openUrl('https://github.com/artificial-cognition-laboratories/terraverse/issues')">Issues on GitHub</Button>
                </div>
            </Section>

            <Section title="Launcher">
                <div class="row">
                    <span class="dim">{{ state.launcherUpdate ? `${state.launcherUpdate.version} is ready to install.` : "No newer launcher found." }}</span>
                    <Button v-if="state.launcherUpdate" size="sm" @click="store.updateLauncher()">Restart to update</Button>
                </div>
            </Section>

            <footer v-if="state.about" class="about">Launcher {{ state.about.launcher }} · {{ state.about.platform }} · built on luanti</footer>
        </div>
    </div>
</template>

<style scoped>
.settings { height: 100%; overflow-y: auto; background: var(--stone-0); }
.column { display: flex; flex-direction: column; gap: 22px; max-width: 600px; margin: 0 auto; padding: 36px 28px 48px; }
h2 { margin: 0; font: 18px var(--font-display); text-transform: uppercase; }
.chev { color: var(--accent); }
.lead { margin: -14px 0 0; color: var(--ink-dim); font-size: 11px; line-height: 1.5; }
.dim { margin: 0; color: var(--ink-faint); font-size: 11px; }
.row { display: flex; align-items: center; gap: 10px; }
.row .dim { flex: 1; }
dl { display: grid; gap: 4px; margin: 0; }
dl div { display: flex; justify-content: space-between; gap: 16px; font-size: 12px; }
/* labels as the game's settings write them: the display face, small and upper case */
dt, .settings :deep(.toggle .label) { font: 12px var(--font-display); text-transform: uppercase; color: var(--ink); }
dd { margin: 0; min-width: 0; overflow-wrap: anywhere; text-align: right; }
.mono { font-family: var(--font); user-select: text; }
.about { color: var(--ink-faint); font-size: 10px; text-transform: uppercase; letter-spacing: 0.12em; }
</style>
