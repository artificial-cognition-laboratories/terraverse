<script setup lang="ts">
import { ref } from "vue"
import { Button, Card, CategoryTile, IconButton, IconRail, ICONS, Panel, Sky } from "../../kit/index.ts"
import PixelIcon from "../../game/PixelIcon.vue"
import { CATEGORY_ICONS } from "../../menu/library/icons.ts"

/** The kit on the night it's made for: every piece over the sky, then a menu composed of them. */
const page = ref<"play" | "library" | "wiki" | "settings">("play")
const rail = [
    { id: "play", label: "Play", ...ICONS.play },
    { id: "library", label: "Library", ...ICONS.library },
    { id: "wiki", label: "Wiki", ...ICONS.wiki },
    { id: "settings", label: "Settings", end: true, ...ICONS.settings },
] as const
const cards = [
    { title: "Materials", seed: "@terraverse/materials", by: "terraverse", text: "The game's own blocks, each made of a substance: granite, clay, oak, iron.", tag: "Matter" },
    { title: "Survival", seed: "@terraverse/survival", by: "terraverse", text: "Hunger, health, night and the long walk to a better tool.", tag: "Survival" },
    { title: "Kilns", seed: "@ash/kilns", by: "ash", text: "Fired clay, brick and glass from heat you have to build and feed.", tag: "Matter" },
]
const categories = (["games", "survival", "building", "machines"] as const).map(id => {
    const icon = CATEGORY_ICONS[id]
    if (!icon) throw new Error(`Kit: no icon for the ${id} category`)
    return { id: id, ...icon }
})
</script>

<template>
    <Sky class="kit">
        <section class="menu">
            <IconRail v-model="page" :items="rail" label="Menu">
                <template #foot>
                    <IconButton label="Profile" side="right"><PixelIcon v-bind="ICONS.profile" style="--u: 3px" /></IconButton>
                </template>
            </IconRail>
            <div class="hero">
                <p class="kicker">» Free and open source · Open beta</p>
                <h1>Terraverse</h1>
                <p class="line">A universe to play in.</p>
                <div class="actions">
                    <Button variant="primary" size="lg">Play »</Button>
                    <Button size="lg">Library</Button>
                </div>
            </div>
        </section>

        <h2>» Buttons</h2>
        <div class="row">
            <Button variant="primary">Primary »</Button>
            <Button>Plain</Button>
            <Button variant="ghost">Ghost</Button>
            <Button variant="danger">Delete</Button>
            <Button busy>Working</Button>
            <Button disabled>Disabled</Button>
            <Button variant="primary" size="sm">Small</Button>
            <Button size="sm">Small</Button>
        </div>

        <h2>» Icons</h2>
        <div class="row">
            <IconButton v-for="(icon, id) in ICONS" :key="id" :label="id" :on="id === 'library'">
                <PixelIcon v-bind="icon" style="--u: 3px" />
            </IconButton>
        </div>

        <h2>» Categories</h2>
        <div class="tiles">
            <CategoryTile v-for="c in categories" :key="c.id" :label="c.id" :rows="c.rows" :palette="c.palette" :count="3" />
        </div>

        <h2>» Cards</h2>
        <div class="cards">
            <Card v-for="c in cards" :key="c.seed" :title="c.title" :seed="c.seed" :by="c.by" :description="c.text" :badges="c.by === 'terraverse' ? ['Official'] : []">
                <template #meta><span>★ 140</span><span>↓ 24k</span><span class="tag">{{ c.tag }}</span></template>
            </Card>
        </div>

        <h2>» Panel</h2>
        <Panel title="Settings" class="panel">
            <template #actions><Button size="sm" variant="ghost">Close</Button></template>
            <p>A pane of glass with one hairline edge. Whatever the page draws behind it is blurred; the game's frame isn't, it's under the page.</p>
            <template #footer><Button>Cancel</Button><Button variant="primary">Save »</Button></template>
        </Panel>
    </Sky>
</template>

<style scoped>
.kit { margin: -28px; padding: 28px 40px 60px; min-height: calc(100% + 56px); }
h2 { margin: 40px 0 14px; font: 11px var(--font-display); letter-spacing: 0.1em; text-transform: uppercase; color: var(--accent-hi); }
.row { display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
.menu { display: grid; grid-template-columns: 84px 1fr; height: 440px; border: 1px solid var(--glass-edge); }
.hero { display: grid; place-content: center; justify-items: center; gap: 10px; text-align: center; }
.kicker { margin: 0; font: 9px var(--font-display); letter-spacing: 0.12em; text-transform: uppercase; color: var(--accent-hi); }
.hero h1 { margin: 0; font: 400 64px var(--font-display); letter-spacing: 0.06em; text-transform: uppercase; color: var(--ink); }
.line { margin: 0 0 18px; color: var(--ink-dim); }
.actions { display: flex; gap: 12px; }
.tiles { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 10px; max-width: 900px; }
.cards { display: grid; grid-template-columns: repeat(3, minmax(0, 260px)); gap: 14px; }
.tag { margin-left: auto; padding: 1px 5px; border: 1px solid var(--glass-edge); }
.panel { width: 460px; }
.panel p { margin: 0; color: var(--ink-dim); font-size: 12px; }
</style>
