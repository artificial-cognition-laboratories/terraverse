<script setup lang="ts">
import { computed } from "vue"
import type { RegistryMod } from "../../bridge/index.ts"
import { Copier, Cover, Monogram } from "../../kit/index.ts"
import PixelIcon from "../../game/PixelIcon.vue"
import { ICONS } from "../../kit/icons.ts"
import { format } from "./format.ts"

/**
 * The top of a mod's page, as the website's (registry/detail/header.vue): its banner with the
 * address on it (a click copies the line that installs it), its mark, name, version and who made
 * it, its words. Where the site has only the star, the game also has what you do with it (the
 * `actions` slot: install, or reload one of your own).
 */
const props = withDefaults(defineProps<{ mod: RegistryMod; yours?: boolean }>(), { yours: false })
const install = Copier()
const command = computed(() => `terra install ${props.mod.name}`)
const title = computed(() => props.mod.title ?? format.bare(props.mod.name))
const STATE = { idle: "copy", copied: "copied", failed: "can't copy" } as const
</script>

<template>
    <header class="header">
        <div class="banner">
            <Cover class="art" :seed="mod.name" :image="mod.banner" :width="120" />
            <button type="button" class="address" :title="command" @click="install.copy(command)">
                <code>{{ mod.name }}</code>
                <span class="state">{{ STATE[install.state.value] }}</span>
            </button>
        </div>
        <div class="title">
            <Monogram :name="mod.name" :image="mod.icon" :size="64" />
            <div class="names">
                <div class="line">
                    <h1>{{ title }}</h1>
                    <span class="badge">v{{ mod.latestVersion }}</span>
                    <span v-if="mod.official" class="badge official">Official</span>
                    <span v-if="yours" class="badge official">Yours</span>
                    <div class="end">
                        <!-- starring isn't on the bridge yet: the count shows, the press waits -->
                        <button v-if="!yours" type="button" class="star" disabled title="Starring isn't wired yet">
                            <PixelIcon :rows="ICONS.star.rows" :palette="ICONS.star.palette" style="--u: 1.6px" />
                            <span>Star</span>
                            <span class="count">{{ format.count(mod.stars) }}</span>
                        </button>
                        <slot name="actions" />
                    </div>
                </div>
                <span class="by">by <b>{{ yours ? "you" : mod.owner }}</b></span>
            </div>
        </div>
        <p v-if="mod.description" class="description">{{ mod.description }}</p>
        <p v-if="mod.deprecated !== null" class="deprecated">Deprecated: {{ mod.deprecated || "its author no longer keeps it." }}</p>
    </header>
</template>

<style scoped>
.header { display: grid; gap: 1.25rem; }
.banner { position: relative; height: 9rem; border: 1px solid var(--glass-edge); }
.art { position: absolute; inset: 0; }
/* the address, copied as an install line: quiet until it's wanted */
.address { position: absolute; top: 0.75rem; right: 0.75rem; display: flex; gap: 0.6rem; align-items: center; padding: 0.35rem 0.6rem; border: 1px solid var(--glass-edge); background: rgb(4 7 11 / 0.55); backdrop-filter: blur(6px); color: var(--ink-dim); font: 0.72rem var(--font); }
.address code { font: inherit; }
.address .state { display: none; color: var(--ink-faint); }
.address:hover { color: var(--ink); border-color: var(--edge-lit-hi); }
.address:hover .state { display: inline; }
.title { position: relative; display: flex; gap: 1.1rem; align-items: center; margin-top: -3.25rem; padding: 0 1.25rem; }
.names { flex: 1; display: grid; gap: 0.35rem; min-width: 0; padding-top: 2.5rem; }
.line { display: flex; flex-wrap: wrap; gap: 0.75rem; align-items: center; }
h1 { margin: 0; color: var(--ink); font: 400 clamp(1.4rem, 3vw, 2rem) var(--font-display); text-transform: uppercase; }
.badge { padding: 0.2rem 0.45rem; border: 1px solid var(--glass-edge); color: var(--ink-dim); font: 0.68rem var(--font); }
.badge.official { border-color: var(--edge-lit-hi); color: var(--accent-hi); font: 0.55rem var(--font-display); text-transform: uppercase; }
/* the star sits at the far end of the title line, level with the name */
.end { display: flex; gap: 0.6rem; align-items: center; margin-left: auto; }
.star { display: inline-flex; align-items: center; gap: 0.55rem; height: 2.1rem; padding: 0 0.75rem; border: 1px solid var(--glass-edge); background: var(--glass); backdrop-filter: blur(8px); color: var(--ink-dim); font: 0.62rem var(--font-display); text-transform: uppercase; }
.star:disabled { cursor: default; }
.star svg { filter: grayscale(1) brightness(0.8); }
.star .count { padding-left: 0.55rem; border-left: 1px solid var(--glass-edge); color: var(--ink-faint); font: 0.72rem var(--font); }
.by { color: var(--ink-faint); font-size: 0.78rem; }
.by b { font-weight: 400; color: var(--accent-hi); }
.description { max-width: 44rem; margin: 0; color: var(--ink-dim); font: 1.05rem/1.6 var(--font-sans); }
.deprecated { margin: 0; padding: 0.75rem 1rem; border: 1px solid rgb(210 153 34 / 0.5); color: var(--warn); font-size: 0.85rem; }
</style>
