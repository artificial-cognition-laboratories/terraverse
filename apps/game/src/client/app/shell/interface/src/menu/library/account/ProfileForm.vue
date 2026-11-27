<script setup lang="ts">
import { computed, reactive, watch } from "vue"
import type { AccountProfile, ProfileEdit, ProfileLink } from "../../../bridge/index.ts"
import { Button, Field, Notice, Section, TextArea } from "../../../kit/index.ts"
import type { EditsHandle } from "./edits.ts"

/**
 * What others see on your page, edited in place: a few words, where you are, your site and up to
 * six links. Saved together; only what changed is sent, and the backend's no is said as it is.
 * Limits match the backend's (arclabs: server/utils/terrafirma-profile.ts).
 */
const props = defineProps<{ profile: AccountProfile; edits: EditsHandle }>()
const emit = defineEmits<{ saved: [profile: AccountProfile] }>()
const LINKS_MAX = 6
const draft = reactive({ bio: "", location: "", website: "", links: [] as ProfileLink[] })

function reset() {
    draft.bio = props.profile.bio ?? ""
    draft.location = props.profile.location ?? ""
    draft.website = props.profile.website ?? ""
    draft.links = props.profile.links.map((l) => ({ label: l.label, url: l.url }))
}
watch(() => props.profile, reset, { immediate: true })

// half-typed links (no label, or no address) aren't saved
const links = computed(() => draft.links.filter((l) => l.label.trim() && l.url.trim()))
const changes = computed<ProfileEdit>(() => {
    const out: ProfileEdit = {}
    if (draft.bio.trim() !== (props.profile.bio ?? "")) out.bio = draft.bio
    if (draft.location.trim() !== (props.profile.location ?? "")) out.location = draft.location
    if (draft.website.trim() !== (props.profile.website ?? "")) out.website = draft.website
    if (JSON.stringify(links.value) !== JSON.stringify(props.profile.links)) out.links = links.value
    return out
})
const dirty = computed(() => Object.keys(changes.value).length > 0)

async function save() {
    const saved = await props.edits.send("account.edit", changes.value)
    if (saved) emit("saved", saved)
}
</script>

<template>
    <div class="form">
        <Section title="About" hint="On your page, under your handle">
            <TextArea v-model="draft.bio" label="Bio" placeholder="What you build, what you play" :maxlength="280" />
            <div class="pair">
                <Field v-model="draft.location" label="Location" placeholder="Optional" :maxlength="64" />
                <Field v-model="draft.website" label="Website" placeholder="https://" :maxlength="200" mono />
            </div>
        </Section>

        <Section title="Links" hint="Where else people find you">
            <div v-for="(link, i) in draft.links" :key="i" class="link">
                <input v-model="link.label" class="label" placeholder="Label" maxlength="32" spellcheck="false">
                <input v-model="link.url" class="url" placeholder="https://" maxlength="200" spellcheck="false">
                <button type="button" class="drop" title="Remove this link" @click="draft.links.splice(i, 1)">✕</button>
            </div>
            <button v-if="draft.links.length < LINKS_MAX" type="button" class="add" @click="draft.links.push({ label: '', url: '' })">+ Add a link</button>
        </Section>

        <Notice v-if="edits.problem.value" tone="danger">{{ edits.problem.value }}</Notice>
        <footer>
            <Button size="sm" :disabled="!dirty || edits.busy.value" @click="reset">Revert</Button>
            <Button variant="primary" size="sm" :disabled="!dirty" :busy="edits.busy.value" @click="save">Save</Button>
        </footer>
    </div>
</template>

<style scoped>
.form { display: grid; gap: 1.75rem; }
.pair { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1rem; margin-top: 1rem; }
.link { display: grid; grid-template-columns: 9rem minmax(0, 1fr) 2.4rem; gap: 0.5rem; margin-bottom: 0.5rem; }
.link input { min-width: 0; height: 2.4rem; padding: 0 0.75rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); background: rgb(4 7 11 / 0.6); outline: none; }
.link input:focus { border-color: var(--edge-lit-hi); }
.link input::placeholder { color: var(--ink-faint); }
.url { font: 0.78rem var(--font); }
.drop { border: 1px solid var(--glass-edge); border-radius: var(--radius); background: none; color: var(--ink-faint); }
.drop:hover { color: var(--danger); border-color: var(--danger); }
.add { justify-self: start; padding: 0.6rem 0.9rem; border: 1px dashed var(--night-4); border-radius: var(--radius); background: none; color: var(--ink-dim); font: 0.6rem var(--font-display); text-transform: uppercase; }
.add:hover { color: var(--ink); border-color: var(--glass-edge-hi); }
footer { display: flex; justify-content: flex-end; gap: 0.6rem; }
</style>
