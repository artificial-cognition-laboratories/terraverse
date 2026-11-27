<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { BridgeError, type HandleAvailability } from "../../../bridge/index.ts"
import { useBridge } from "../../../bridge/use.ts"
import { Button, Field, Notice, Section } from "../../../kit/index.ts"
import type { EditsHandle } from "./edits.ts"

/**
 * Taking a new handle. It's the scope of everything you've published, so it's said what breaks
 * before it's asked, and asked twice: the name, checked as it's typed, then typed again to do it.
 * In the menu only, as this account's worlds move to the new name's profile with it.
 */
const props = defineProps<{ handle: string; inMenu: boolean; edits: EditsHandle }>()
const emit = defineEmits<{ renamed: [] }>()
const bridge = useBridge()
const wanted = ref("")
const confirm = ref("")
const confirming = ref(false)
const check = ref<HandleAvailability | null>(null)
const checking = ref(false)
const name = computed(() => wanted.value.trim().toLowerCase().replace(/^@/, ""))
const same = computed(() => name.value === props.handle)

// asked a moment after typing stops; only the answer for what's typed now is kept
let timer: ReturnType<typeof setTimeout> | undefined
watch(name, (n) => {
    clearTimeout(timer)
    check.value = null
    confirming.value = false
    confirm.value = ""
    if (!n || same.value) return
    checking.value = true
    timer = setTimeout(async () => {
        // advice only, kept off the page's one-at-a-time changes: a refusal is its reason
        const answer: HandleAvailability = await bridge.send("account.available", { username: n })
            .catch((e: unknown) => {
                if (!(e instanceof BridgeError) || !e.refused) throw e
                return { available: false as const, reason: e.message }
            })
        if (n !== name.value) return
        checking.value = false
        check.value = answer
    }, 400)
})
const ready = computed(() => check.value?.available === true && !checking.value)

async function rename() {
    if (await props.edits.send("account.rename", { username: name.value })) {
        wanted.value = ""
        emit("renamed")
    }
}
</script>

<template>
    <div class="form">
        <Section title="Handle" hint="Your name everywhere: friends, worlds and the registry">
            <Field v-model="wanted" label="New handle" :placeholder="handle" :maxlength="30" mono>
                <template #hint>
                    <span v-if="checking" class="state">Checking…</span>
                    <span v-else-if="check?.available" class="state ok">@{{ name }} is free</span>
                    <span v-else-if="check" class="state no">{{ check.reason }}</span>
                    <span v-else-if="same" class="state">That's you now</span>
                </template>
            </Field>
            <p class="rule">3–30 characters: lowercase letters, digits, dots, hyphens or underscores.</p>
        </Section>

        <Notice tone="warn">
            Renaming moves everything to <strong>@{{ ready ? name : "the new name" }}</strong>: your mods are published under it,
            friends see it, and this machine's worlds go with it. Links to <strong>@{{ handle }}</strong> and
            <strong>terra://@{{ handle }}/…</strong> addresses stop working, and someone else can take @{{ handle }}.
        </Notice>
        <Notice v-if="!inMenu" tone="info">Leave your world to rename: its folder moves with you.</Notice>
        <Notice v-if="edits.problem.value" tone="danger">{{ edits.problem.value }}</Notice>

        <div v-if="confirming" class="confirm">
            <Field v-model="confirm" :label="`Type @${name} to confirm`" :placeholder="`@${name}`" :maxlength="31" mono />
            <footer>
                <Button size="sm" @click="confirming = false; confirm = ''">Cancel</Button>
                <Button variant="danger" size="sm" :disabled="confirm.trim().replace(/^@/, '') !== name" :busy="edits.busy.value"
                    @click="rename">Rename to @{{ name }}</Button>
            </footer>
        </div>
        <footer v-else>
            <Button variant="primary" size="sm" :disabled="!ready || !inMenu" @click="confirming = true">Rename…</Button>
        </footer>
    </div>
</template>

<style scoped>
.form { display: grid; gap: 1.25rem; }
.state { color: var(--ink-faint); font: 0.72rem var(--font); }
.state.ok { color: var(--accent-hi); }
.state.no { color: var(--warn); }
.rule { margin: 0.5rem 0 0; color: var(--ink-faint); font-size: 0.72rem; }
.confirm { display: grid; gap: 0.9rem; padding: 1rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(255 255 255 / 0.02); }
footer { display: flex; justify-content: flex-end; gap: 0.6rem; }
</style>
