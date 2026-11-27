<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { AccountProfile } from "../../../bridge/index.ts"
import { useBridge } from "../../../bridge/use.ts"
import { useCache } from "../../../cache/index.ts"
import { useStore } from "../../../game/store.ts"
import { Button, DetailHeader, DetailLayout, ICONS, Notice, Shelf, SideCard, SideNav, SideStats } from "../../../kit/index.ts"
import type { ActionsHandle } from "../../../online/actions.ts"
import Accounts from "../../../online/Accounts.vue"
import { format } from "../format.ts"
import Published from "../Published.vue"
import { Edits } from "./edits.ts"
import HandleForm from "./HandleForm.vue"
import ProfileForm from "./ProfileForm.vue"

/**
 * Your account, kept from the game rather than the site: your page as others see it (edited in
 * place), what you've published, your handle, and the accounts this machine remembers. Billing,
 * keys and the rest of ArcLabs stay on the site, one link away.
 */
const props = defineProps<{ handle: string; actions: ActionsHandle }>()
const emit = defineEmits<{ mod: [name: string]; page: [handle: string] }>()
type Part = "profile" | "published" | "handle" | "machine"
const bridge = useBridge()
const store = useStore()
const edits = Edits({ bridge: bridge })
const owner = useCache().cached("registry.owner")
const part = ref<Part>("profile")
const profile = ref<AccountProfile | null>(null)
const problem = ref<string | null>(null)
const renaming = ref(false)

// read again whenever the handle changes: signing in as another, or a rename landing
watch(() => props.handle, async (handle) => {
    renaming.value = false
    void owner.run({ scope: `@${handle}` })
    problem.value = null
    profile.value = await edits.send("account.profile", {})
    if (!profile.value) problem.value = edits.problem.value
}, { immediate: true })

const listings = computed(() => owner.data.value?.listings ?? [])
const inMenu = computed(() => store.mode === "menu")
const parts = computed(() => [
    { id: "profile" as const, label: "Profile", icon: ICONS.profile },
    { id: "published" as const, label: "Published", icon: ICONS.mods, count: listings.value.length },
    { id: "handle" as const, label: "Handle", icon: ICONS.readme },
    { id: "machine" as const, label: "This machine", icon: ICONS.players },
])
const stats = computed(() => [
    { icon: ICONS.mods, value: format.count(listings.value.length), label: "published" },
    { icon: ICONS.star, value: format.count(owner.data.value?.stars ?? 0), label: "stars" },
    { icon: ICONS.download, value: format.count(owner.data.value?.installs ?? 0), label: "installs" },
])
const friends = computed(() => store.social?.view?.friends.length ?? null)
const SITE = "https://terraverse.arclabs.it"
</script>

<template>
    <DetailLayout :back="null" :ready="profile !== null" :problem="problem">
        <DetailHeader :seed="`@${handle}`" :mark="handle" :title="`@${handle}`" :description="profile?.bio ?? null">
            <template #actions>
                <Button size="sm" @click="emit('page', handle)">As others see it »</Button>
            </template>
            <template #by>{{ listings.length }} published · ★ {{ format.count(owner.data.value?.stars ?? 0) }} · ↓ {{ format.count(owner.data.value?.installs ?? 0) }}</template>
        </DetailHeader>

        <ProfileForm v-if="part === 'profile' && profile" :profile="profile" :edits="edits" @saved="(p) => (profile = p)" />
        <Shelf v-else-if="part === 'published'" title="Published" :cols="null">
            <Published :listings="listings" :reading="owner.busy.value && !owner.data.value" :self="true" @open="(name) => emit('mod', name)" />
            <Notice v-if="owner.problem.value && !owner.data.value" tone="danger">The registry couldn't be reached: {{ owner.problem.value }}</Notice>
        </Shelf>
        <template v-else-if="part === 'handle'">
            <Notice v-if="renaming" tone="info">Renamed. Signing in under the new handle…</Notice>
            <HandleForm v-else :handle="handle" :in-menu="inMenu" :edits="edits" @renamed="renaming = true" />
        </template>
        <Shelf v-else-if="part === 'machine'" title="Accounts on this machine" :cols="null">
            <Accounts :actions="actions" />
        </Shelf>

        <template #side>
            <SideCard>
                <SideNav v-model="part" :items="parts" />
                <SideStats :items="stats" />
                <dl class="meta">
                    <div v-if="profile"><dt>Member since</dt><dd>{{ format.date(profile.createdAt) }}</dd></div>
                    <div v-if="friends !== null"><dt>Friends</dt><dd>{{ friends }}</dd></div>
                    <div v-if="profile?.location"><dt>Location</dt><dd>{{ profile.location }}</dd></div>
                </dl>
                <button type="button" class="site" @click="actions.send('open', { url: SITE })">Manage on the site »</button>
            </SideCard>
        </template>
    </DetailLayout>
</template>

<style scoped>
:deep(.shelf header) { margin-top: 0; }
.meta { display: grid; gap: 0.45rem; margin: 0; font-size: 0.75rem; }
.meta div { display: flex; justify-content: space-between; gap: 1rem; }
.meta dt { color: var(--ink-faint); }
.meta dd { margin: 0; min-width: 0; overflow: hidden; color: var(--ink-dim); text-align: right; text-overflow: ellipsis; white-space: nowrap; }
.site { justify-self: start; padding: 0; border: 0; background: none; color: var(--accent-hi); font: 0.6rem var(--font-display); text-transform: uppercase; }
.site:hover { color: var(--ink); }
</style>
