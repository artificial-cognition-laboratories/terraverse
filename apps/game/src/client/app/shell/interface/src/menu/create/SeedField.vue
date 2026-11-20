<script setup lang="ts">
import { computed, ref, watch } from "vue"
import PixelIcon from "../../game/PixelIcon.vue"
import { Button, Field } from "../../kit/index.ts"
import { WorldCodeError, decodeWorldCode, encodeWorldCode, randomSeed, surprise } from "../../worldgen/config.ts"
import { Copier } from "./copy.ts"
import type { WorldDraftHandle } from "./draft.ts"

/**
 * The seed, in either form: any text is a seed at the current settings; a
 * pasted world code (TF1.…) loads that exact land, settings and all.
 */
const props = defineProps<{ draft: WorldDraftHandle }>()

const DICE = ["KKKKKKK", "K.....K", "K.K...K", "K..K..K", "K...K.K", "K.....K", "KKKKKKK"] as const
const STAR = ["...K...", "...K...", "KKKKKKK", ".KKKKK.", "..KKK..", ".KK.KK.", "K.....K"] as const

const text = ref(props.draft.config.seed)
const note = ref<{ tone: "ok" | "bad"; text: string } | null>(null)
const code = computed(() => encodeWorldCode(props.draft.config))
const copier = Copier()

// The seed can change from outside (new seed, surprise): keep the field in step.
watch(() => props.draft.config.seed, (seed) => {
    if (seed !== text.value) text.value = seed
})

watch(text, (value) => {
    note.value = null
    if (!/^TF\d+\./.test(value.trim())) {
        props.draft.config.seed = value
        return
    }
    try {
        const config = decodeWorldCode(value)
        Object.assign(props.draft.config, config)
        props.draft.spawn = null
        text.value = config.seed
        note.value = { tone: "ok", text: "Loaded the land settings from a world code." }
    } catch (err) {
        if (!(err instanceof WorldCodeError)) throw err
        note.value = { tone: "bad", text: err.message }
    }
})

function surpriseMe() {
    Object.assign(props.draft.config, surprise(), { seed: randomSeed() })
    note.value = { tone: "ok", text: "New land with random settings — see Body · Land & water." }
}
</script>

<template>
    <div class="seed">
        <Field v-model="text" label="Seed" placeholder="any text, or paste a world code" mono :maxlength="200">
            <template #action>
                <Button size="sm" title="New seed, same settings" @click="draft.config.seed = randomSeed()">
                    <PixelIcon :rows="DICE" :palette="{ K: 'currentColor' }" style="--u: 2px" />
                </Button>
                <Button size="sm" title="Surprise me: new seed and random settings" @click="surpriseMe">
                    <PixelIcon :rows="STAR" :palette="{ K: 'currentColor' }" style="--u: 2px" />
                </Button>
                <Button size="sm" :title="`Copy ${code}`" @click="copier.copy(code)">
                    {{ copier.state.value === 'copied' ? 'Copied' : copier.state.value === 'failed' ? 'Failed' : 'Share' }}
                </Button>
            </template>
        </Field>
        <p v-if="note" class="note" :class="note.tone">{{ note.text }}</p>
    </div>
</template>

<style scoped>
.seed { display: flex; flex-direction: column; gap: 6px; }
.note { margin: 0; font-size: 11px; }
.ok { color: var(--accent-hi); }
.bad { color: var(--danger); }
</style>
