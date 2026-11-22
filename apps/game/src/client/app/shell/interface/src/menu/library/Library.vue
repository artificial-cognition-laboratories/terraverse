<script setup lang="ts">
import { computed, ref } from "vue"
import type { LocalMod, PublicServer, RegistrySearch } from "../../bridge/index.ts"
import { Button, WorkbenchPanel } from "../../kit/index.ts"
import type { Screen } from "../screens.ts"
import Discover from "./Discover.vue"
import { RAIL, type Page } from "./icons.ts"
import ModPage from "./ModPage.vue"
import Mods from "./Mods.vue"
import Servers from "./Servers.vue"

/**
 * The library: Discover, Mods and Servers on its rail (the registry's, and the mods you're
 * making), and a mod's own page opened over whichever of them it was opened from.
 */
const emit = defineEmits<{ go: [screen: Screen] }>()
const page = ref<Page>("discover")
const mod = ref<string | null>(null)
/** One of your own mods, opened: its page instead of a registry mod's. */
const local = ref<LocalMod | null>(null)
const modSearch = ref<Partial<RegistrySearch>>({})
const server = ref<string | null>(null)
const section = computed(() => local.value?.name ?? mod.value ?? RAIL.find((item) => item.id === page.value)!.label)

function browse(search: Partial<RegistrySearch>) {
    modSearch.value = search
    mod.value = null
    page.value = "mods"
}
function servers(chosen: PublicServer | null) {
    server.value = chosen?.id ?? null
    mod.value = null
    page.value = "servers"
}
function go(next: Page) {
    mod.value = null
    local.value = null
    page.value = next
}
</script>

<template>
    <WorkbenchPanel :group="page" title="Library" :section="section" :groups="RAIL" rail-label="Library"
        @update:group="go">
        <template #actions><Button size="sm" @click="emit('go', 'home')">Back</Button></template>
        <template #workspace>
            <div class="stage">
                <ModPage v-if="local" :name="local.name" :local="local" :back="RAIL.find((item) => item.id === page)!.label" @back="local = null" />
                <ModPage v-else-if="mod" :name="mod" :back="RAIL.find((item) => item.id === page)!.label"
                    @back="mod = null" @open="(name) => (mod = name)" />
                <Discover v-else-if="page === 'discover'" @open="(name) => (mod = name)" @browse="browse" @servers="servers" />
                <Mods v-else-if="page === 'mods'" :start="modSearch" @open="(name) => (mod = name)" @open-local="(m) => (local = m)" />
                <Servers v-else :start="server" />
            </div>
        </template>
    </WorkbenchPanel>
</template>

<style scoped>
.stage { height: 100%; min-height: 0; overflow-y: auto; scrollbar-gutter: stable both-edges; }
.stage:has(> .mods, > .servers) { overflow: hidden; }
</style>
