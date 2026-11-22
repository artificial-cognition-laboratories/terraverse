<script setup lang="ts" generic="T extends string">
import IconRail from "./IconRail.vue"
import RailBack from "./RailBack.vue"

/**
 * A full screen you browse (the library): the game's rail of pages down the left, the way back
 * at its foot; along the top a bar (the website's subnav, apps/site shared/subnav.vue) saying
 * where you are, the search on its right; then the page held to the site's measure as it scrolls. Behind it all is
 * the menu's own scene, dimmed and frosted: the wash is solid enough that the shell blurs the
 * frame under it (theme.css, --glass). A task stays a Panel.
 *
 * `fill`: the page is the whole area, unmeasured and unscrolled, and unwashed: it lays out and
 * paints its own columns, and leaves a native scene its hole (the wiki).
 */
withDefaults(defineProps<{
    pages: readonly { id: T; label: string; shortLabel?: string; rows: readonly string[]; palette: Record<string, string> }[]
    label: string; section: string; fill?: boolean; back?: string
}>(), { fill: false, back: "Back" })
defineEmits<{ back: [] }>()
const page = defineModel<T>("page", { required: true })
</script>

<template>
    <!-- data-full-screen: the HUD steps out from under it (game/Hud) -->
    <div class="screen" :class="{ fill }" data-full-screen>
        <IconRail v-model="page" class="rail" :items="pages" :label="label">
            <template #foot>
                <RailBack :label="back" @back="$emit('back')" />
            </template>
        </IconRail>
        <div class="subnav">
            <div class="inner">
                <h1><span class="chev">»</span> {{ label }} <span class="sep">/</span> <span class="section">{{ section }}</span></h1>
                <slot name="search" />
            </div>
        </div>
        <div v-if="fill" class="whole"><slot /></div>
        <div v-else class="scroll"><div class="measure"><slot /></div></div>
    </div>
</template>

<style scoped>
.screen { position: absolute; inset: 0; display: grid; grid-template-columns: 84px minmax(0, 1fr); grid-template-rows: 3.25rem minmax(0, 1fr); background: var(--wash); }
.screen.fill { background: none; }
.whole { min-width: 0; min-height: 0; }
.rail { grid-row: 1 / 3; }
.subnav { border-bottom: 1px solid var(--glass-edge); background: var(--side); }
/* filled, the bar spans the page as its columns do, not the measure */
.fill .inner { max-width: none; }
.inner { display: flex; align-items: center; justify-content: space-between; gap: 2rem; max-width: 1200px; height: 100%; margin: 0 auto; padding: 0 2rem; }
h1 { min-width: 0; margin: 0; overflow: hidden; font: 400 0.8rem var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; text-overflow: ellipsis; white-space: nowrap; color: var(--ink); }
.chev { color: var(--accent-hi); }
.sep { color: var(--ink-faint); }
.section { color: var(--ink-dim); }
.subnav :deep(.search-box) { width: min(26rem, 50%); }
/* both gutters kept, so the page stays centred as the scrollbar comes and goes */
.scroll { overflow-y: auto; scrollbar-gutter: stable both-edges; }
.measure { max-width: 1200px; margin: 0 auto; padding: 2.5rem 2rem 6rem; }
</style>
