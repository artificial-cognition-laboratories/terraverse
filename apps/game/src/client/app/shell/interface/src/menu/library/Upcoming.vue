<script setup lang="ts">
import { ref } from "vue"
import { ListEmpty, ResultsHead, SideCard, SideNav } from "../../kit/index.ts"
import type { Icon } from "../../kit/icons.ts"

/**
 * A kind of the library the registry doesn't hold yet (packs, worlds), laid out as every browse
 * is: its own scopes down the side card beside the results, which say plainly it isn't there yet.
 */
const props = defineProps<{ title: string; scopes: readonly { id: string; label: string; icon: Icon }[]; empty: string; note: string }>()
const scope = ref(props.scopes[0]?.id ?? "")
</script>

<template>
    <div class="browse">
        <SideCard class="side">
            <SideNav v-model="scope" :items="scopes" />
            <p class="note">{{ note }}</p>
        </SideCard>
        <div class="results">
            <ResultsHead :title="title" count="Not in the registry yet" />
            <ListEmpty :title="empty"><slot /></ListEmpty>
        </div>
    </div>
</template>

<style scoped>
.browse { display: grid; grid-template-columns: 17rem minmax(0, 1fr); gap: 3rem; align-items: start; }
.side { position: sticky; top: 2.5rem; }
.note { margin: 0; padding: 0.9rem 1rem; color: var(--ink-faint); font: 0.75rem/1.5 var(--font-sans); }
@media (max-width: 900px) { .browse { grid-template-columns: minmax(0, 1fr); } .side { position: static; } }
</style>
