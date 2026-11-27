<script setup lang="ts">
import { computed, watch } from "vue"
import { useCache } from "../cache/index.ts"
import { useStore } from "../game/store.ts"
import { Button, DetailHeader, DetailLayout, ICONS, KeptNote, Notice, Shelf, SideCard, SideNav, SideStats } from "../kit/index.ts"
import Published from "../menu/library/Published.vue"
import { format } from "../menu/library/format.ts"
import { presenceLine } from "../social/presence.ts"
import type { ActionsHandle } from "./actions.ts"
import { places } from "./places.ts"

/**
 * A player's page, as the website's owner page (apps/site registry/owner): their banner, handle
 * and numbers, what they've published as cards, and the same side card (Overview, the numbers,
 * when they last updated and where it's filed). Beside the site's, the game has what's between
 * you: where they are, and what you can do with them (join, invite, add, answer, remove), and on
 * yours the accounts this machine remembers.
 */
/** `back`: null on your own page, opened from the rail with nowhere to go back to. */
const props = defineProps<{ handle: string; back: string | null; actions: ActionsHandle }>()
const emit = defineEmits<{ back: []; mod: [name: string] }>()
const store = useStore()
const owner = useCache().cached("registry.owner")
const view = computed(() => store.social?.view ?? null)
const self = computed(() => store.account?.status === "signedIn" && store.account.username === props.handle)
const friend = computed(() => view.value?.friends.find((f) => f.username === props.handle) ?? null)
const incoming = computed(() => view.value?.incoming.find((p) => p.username === props.handle) ?? null)
const outgoing = computed(() => view.value?.outgoing.find((p) => p.username === props.handle) ?? null)
const invite = computed(() => view.value?.invites.find((i) => i.from.username === props.handle) ?? null)
const name = computed(() => self.value && store.account?.status === "signedIn" ? store.account.name
    : friend.value?.name ?? incoming.value?.name ?? null)
const listings = computed(() => owner.data.value?.listings ?? [])
const updated = computed(() => listings.value.map((l) => l.updatedAt).sort().at(-1) ?? null)
const categories = computed(() => [...new Set(listings.value.flatMap((l) => l.categories))])
const reading = computed(() => owner.busy.value && !owner.data.value)
const inMenu = computed(() => store.social?.me.state === "menu")
const canInvite = computed(() => !!store.social?.joinable && !!friend.value?.presence && friend.value.presence.world !== store.social.me.world)
const stats = computed(() => [
    { icon: ICONS.mods, value: format.count(listings.value.length), label: "published" },
    { icon: ICONS.star, value: format.count(owner.data.value?.stars ?? 0), label: "stars" },
    { icon: ICONS.download, value: format.count(owner.data.value?.installs ?? 0), label: "installs" },
])
const contents = computed(() => [{ id: "overview" as const, label: "Overview", icon: ICONS.mods, count: listings.value.length }])
const relation = computed(() => self.value ? "You" : friend.value ? "Friend" : incoming.value ? "Wants to be friends"
    : outgoing.value ? "Request sent" : "Not a friend")
const SITE = "https://terraverse.arclabs.it"
watch(() => props.handle, (handle) => { void owner.run({ scope: `@${handle}` }) }, { immediate: true })
</script>

<template>
    <DetailLayout :back="back" :ready="true" @back="emit('back')">
        <template #above>
            <KeptNote v-if="owner.problem.value && owner.at.value !== null" :at="owner.at.value" :problem="owner.problem.value" />
        </template>
        <DetailHeader :seed="`@${handle}`" :mark="handle" :title="`@${handle}`">
            <template #badges>
                <span v-if="owner.data.value?.official" class="badge lit">Official</span>
            </template>
            <template #actions>
                <template v-if="self">
                    <Button size="sm" @click="actions.send('open', { url: `${SITE}/registry/@${handle}` })">On the site »</Button>
                </template>
                <template v-else-if="friend">
                    <Button v-if="canInvite" size="sm" :disabled="actions.busy.value" @click="actions.invite(friend.id)">Invite</Button>
                    <Button v-if="places.joinable(friend)" variant="primary" size="sm" :disabled="actions.busy.value || !inMenu"
                        :title="inMenu ? undefined : 'Leave your world first'" @click="actions.joinFriend(friend)">{{ actions.joining.value === friend.id ? "Joining…" : "Join »" }}</Button>
                </template>
                <template v-else-if="incoming">
                    <Button size="sm" :disabled="actions.busy.value" @click="actions.remove(incoming.id)">Decline</Button>
                    <Button variant="primary" size="sm" :disabled="actions.busy.value" @click="actions.accept(incoming.id)">Accept</Button>
                </template>
                <Button v-else-if="outgoing" size="sm" :disabled="actions.busy.value" @click="actions.remove(outgoing.id)">Cancel request</Button>
                <Button v-else-if="store.account?.status === 'signedIn'" variant="primary" size="sm" :disabled="actions.busy.value"
                    @click="actions.request(handle)">Add friend</Button>
            </template>
            <template #by>{{ listings.length }} published · ★ {{ format.count(owner.data.value?.stars ?? 0) }} · ↓ {{ format.count(owner.data.value?.installs ?? 0) }}</template>
        </DetailHeader>
        <Notice v-if="actions.problem.value" tone="danger">{{ actions.problem.value }}</Notice>
        <Notice v-if="invite && inMenu" tone="info">@{{ handle }} invited you to {{ invite.world }}.
            <button type="button" class="link" :disabled="actions.busy.value" @click="actions.joinFriend(invite.from)">Join »</button></Notice>

        <!-- the profile: what the side card doesn't say, and what's between you (the game's, not the site's) -->
        <Shelf title="Overview" :cols="null">
            <dl class="details">
                <div><dt>Handle</dt><dd class="mono">@{{ handle }}</dd></div>
                <div v-if="name"><dt>Name</dt><dd>{{ name }}</dd></div>
                <div><dt>You are</dt><dd :class="{ lit: self || friend }">{{ relation }}</dd></div>
                <div v-if="friend"><dt>Now</dt><dd :class="{ lit: friend.presence }">{{ presenceLine(friend.presence) }}</dd></div>
                <div v-if="self && view"><dt>Friends</dt><dd>{{ view.friends.length }}</dd></div>
            </dl>
            <div v-if="friend" class="foot">
                <button type="button" class="link warn" :disabled="actions.busy.value" @click="actions.remove(friend.id)">Remove friend</button>
            </div>
        </Shelf>

        <Shelf title="Published" :cols="null">
            <Published :listings="listings" :reading="reading" :self="self" @open="(name) => emit('mod', name)" />
            <Notice v-if="owner.problem.value && !owner.data.value" tone="danger">The registry couldn't be reached: {{ owner.problem.value }}</Notice>
        </Shelf>

        <template #side>
            <SideCard>
                <SideNav :model-value="'overview'" :items="contents" />
                <SideStats :items="stats" />
                <dl class="meta">
                    <div><dt>Updated</dt><dd>{{ updated ? format.date(updated) : "—" }}</dd></div>
                    <div><dt>{{ categories.length === 1 ? "Category" : "Categories" }}</dt><dd class="filed">{{ categories.join(", ") }}</dd></div>
                </dl>
            </SideCard>
        </template>
    </DetailLayout>
</template>

<style scoped>
/* the shelves sit in the page's own gap, not the site's front-page margin */
:deep(.shelf header) { margin-top: 0; }
.details { display: grid; gap: 0.55rem; margin: 0; }
.details div { display: flex; justify-content: space-between; gap: 1rem; font-size: 0.8rem; }
.details dt { color: var(--ink-faint); }
.details dd { margin: 0; min-width: 0; overflow: hidden; color: var(--ink-dim); text-align: right; text-overflow: ellipsis; white-space: nowrap; }
.details dd.lit { color: var(--accent-hi); }
.mono { font-family: var(--font); }
.foot { display: flex; justify-content: flex-end; margin-top: 0.9rem; }
.link { padding: 0; border: 0; background: none; color: var(--accent-hi); font: inherit; }
.link:hover:not(:disabled) { color: var(--ink); }
.link.warn { color: var(--warn); font: 0.6rem var(--font-display); text-transform: uppercase; }
.meta { display: grid; gap: 0.45rem; margin: 0; font-size: 0.75rem; }
.meta div { display: flex; justify-content: space-between; gap: 1rem; }
.meta dt { color: var(--ink-faint); }
.meta dd { margin: 0; min-width: 0; color: var(--ink-dim); text-align: right; }
.filed { color: var(--accent-hi) !important; }
</style>
