<script setup lang="ts">
import { onMounted, ref } from "vue"
import { Button, Field, Notice, Panel } from "@game/kit/index.ts"
import type { Report } from "../backend/index.ts"
import { useStore } from "../use.ts"

/**
 * A problem report, to ArcLabs. After a crash (`crash` set) it's offered at once;
 * by hand (from Settings) it carries the last session's log and no dump.
 */
const props = defineProps<{ crash: { session: string; version: string; code: number } | null }>()
const emit = defineEmits<{ close: [] }>()
const store = useStore()
const report = ref<Report | null>(null)
const note = ref("")
const sending = ref(false)
const showLog = ref(false)

onMounted(async () => { report.value = await store.collectReport(props.crash?.session ?? null) })

async function send() {
    if (!report.value) return
    sending.value = true
    try {
        await store.submitReport(report.value.session, note.value)
        emit("close")
    } finally {
        sending.value = false
    }
}
async function skip() {
    if (props.crash) await store.dismissCrash()
    emit("close")
}
</script>

<template>
    <div class="scrim">
        <Panel :title="crash ? 'The game crashed' : 'Report a problem'" class="dialog">
            <div class="body">
                <Notice v-if="crash" tone="danger">{{ crash.version }} stopped unexpectedly (exit code {{ crash.code }}).</Notice>
                <p>{{ crash ? "Sending a report helps us find what went wrong." : "Tell us what happened; the last session's log goes with it." }}</p>
                <template v-if="report">
                    <ul>
                        <li><span>System</span>{{ report.system }}</li>
                        <li><span>Version</span>{{ report.version }}</li>
                        <li><span>Log</span>session {{ report.session }} <button class="link" @click="showLog = !showLog">{{ showLog ? "hide" : "show" }}</button></li>
                        <li v-if="report.dump"><span>Crash dump</span>included</li>
                    </ul>
                    <pre v-if="showLog">{{ report.log }}</pre>
                    <Field v-model="note" :label="crash ? 'What were you doing? (optional)' : 'What went wrong?'" />
                </template>
                <p v-else class="dim">Gathering the report…</p>
            </div>
            <template #footer>
                <Button size="sm" @click="store.openDataFolder()">Open data folder</Button>
                <span class="grow" />
                <Button size="sm" @click="skip">{{ crash ? "Don't send" : "Cancel" }}</Button>
                <Button variant="primary" :disabled="!report || sending || (!crash && !note.trim())" @click="send">
                    {{ sending ? "Sending…" : "Send report" }}
                </Button>
            </template>
        </Panel>
    </div>
</template>

<style scoped>
.scrim { position: absolute; inset: 0; z-index: 10; display: grid; place-items: center; padding: 16px; background: rgb(3 8 15 / 0.7); }
.dialog { width: 580px; max-width: 100%; }
.body { display: flex; flex-direction: column; gap: 12px; }
p { margin: 0; color: var(--ink-dim); font-size: 13px; }
ul { margin: 0; padding: 0; list-style: none; display: grid; gap: 4px; font-size: 12px; }
li span { display: inline-block; width: 96px; color: var(--ink-faint); }
pre { margin: 0; padding: 8px 10px; max-height: 160px; overflow: auto; background: var(--stone-0); box-shadow: var(--sunk); font-size: 11px; white-space: pre-wrap; user-select: text; }
.link { padding: 0; margin-left: 6px; border: 0; background: none; color: var(--accent-hi); font-size: 12px; }
.dim { color: var(--ink-faint); font-size: 11px; }
.grow { flex: 1; }
</style>
