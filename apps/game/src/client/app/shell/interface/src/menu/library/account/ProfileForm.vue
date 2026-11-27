<script setup lang="ts">
import { computed, reactive, watch } from "vue"
import type { AccountProfile, ProfileEdit, ProfileLink } from "../../../bridge/index.ts"
import { Button, Field, FormRow, Notice, TextArea } from "../../../kit/index.ts"
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
        <div class="rows">
            <FormRow title="Handle" description="Your name everywhere: friends, worlds and the registry. Chosen once, when you first signed in; it can't change.">
                <output class="handle">@{{ profile.username }}</output>
            </FormRow>
            <FormRow title="Bio" description="A line or two under your handle: what you build, what you play.">
                <TextArea v-model="draft.bio" placeholder="What you build, what you play" :maxlength="280" :rows="4" />
            </FormRow>
            <FormRow title="Location" description="Optional. Where you play from.">
                <Field v-model="draft.location" placeholder="Optional" :maxlength="64" />
            </FormRow>
            <FormRow title="Website" description="Optional. An https:// link, shown on your page.">
                <Field v-model="draft.website" placeholder="https://" :maxlength="200" mono />
            </FormRow>
            <FormRow title="Links" :description="`Where else people find you: GitHub, Discord, a channel. Up to ${LINKS_MAX}.`">
                <div v-for="(link, i) in draft.links" :key="i" class="link">
                    <input v-model="link.label" class="label" placeholder="Label" maxlength="32" spellcheck="false">
                    <input v-model="link.url" class="url" placeholder="https://" maxlength="200" spellcheck="false">
                    <button type="button" class="drop" title="Remove this link" @click="draft.links.splice(i, 1)">✕</button>
                </div>
                <button v-if="draft.links.length < LINKS_MAX" type="button" class="add" @click="draft.links.push({ label: '', url: '' })">+ Add a link</button>
            </FormRow>
        </div>

        <Notice v-if="edits.problem.value" tone="danger">{{ edits.problem.value }}</Notice>
        <footer>
            <Button size="sm" :disabled="!dirty || edits.busy.value" @click="reset">Revert</Button>
            <Button variant="primary" size="sm" :disabled="!dirty" :busy="edits.busy.value" @click="save">Save</Button>
        </footer>
    </div>
</template>

<style scoped>
.form { display: grid; gap: 1.25rem; }
.handle { display: flex; align-items: center; height: 2.4rem; padding: 0 0.75rem; border: 1px solid var(--glass-edge); border-radius: var(--radius); color: var(--ink-dim); font: 0.78rem var(--font); }
.link { display: grid; grid-template-columns: 6.5rem minmax(0, 1fr) 2.4rem; gap: 0.5rem; }
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
