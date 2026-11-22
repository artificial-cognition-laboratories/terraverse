<script setup lang="ts">
import { ref } from "vue"
import { Card, CategoryTile, Cover, FacetList, FileTree, FilterChips, Markdown, Monogram, PageHead, Shelf } from "../../kit/index.ts"
import Library from "../../menu/library/Library.vue"
import ModPage from "../../menu/library/ModPage.vue"
import Mods from "../../menu/library/Mods.vue"
import Servers from "../../menu/library/Servers.vue"
import { CATEGORY_ICONS } from "../../menu/library/icons.ts"
import Specimen from "../Specimen.vue"

const tab = ref<"overview" | "versions">("overview")
const facets = ref<string[]>(["games"])
const chips = ref([{ id: "q", label: "“rails”" }, { id: "kind", label: "Mods" }, { id: "machines", label: "Machines" }])
const file = ref<string | null>("server/main.ts")
const FILES = [{ path: "package.json", size: 412 }, { path: "README.md", size: 2100 }, { path: "server/main.ts", size: 4210 }, { path: "server/rules.ts", size: 2380 }, { path: "assets/icon.png", size: 2048 }]
const README = "## Kilns\n\nFired clay, **brick** and glass from heat you build and feed.\n\n- Needs `@terraverse/craft`\n- Works on any planet\n\n```sh\nterra add @ash/kilns\n```\n\n> Links show where they go: [docs](https://terraverse.dev)"
</script>

<template>
    <div class="grid">
        <Specimen name="Library" note="the whole panel against the mock registry" wide>
            <Library class="whole" />
        </Specimen>
        <Specimen name="ModPage" note="a mod's own page: head on the page, README, facts and source beside it" wide>
            <div class="framed"><ModPage name="@ash/kilns" back="Mods" /></div>
        </Specimen>
        <Specimen name="Mods" note="browsing: filters, chips, results" wide>
            <div class="framed tall"><Mods :start="{ categories: ['machines'] }" /></div>
        </Specimen>
        <Specimen name="Servers" note="filters, rows, the chosen one open" wide>
            <div class="framed tall"><Servers start="iron-valley" /></div>
        </Specimen>
        <Specimen name="Card" note="cover, mark, name, author, two lines, meta; badges on the cover">
            <Card class="card" title="Iron Rails" seed="@wren/rails" by="wren" description="Track, points and wagons on Jolt. Hauling ore stops being a crawl." :badges="['Official']">
                <template #meta><span>★ 301</span><span>↓ 5.5k</span><span>v1.0.0</span></template>
            </Card>
            <Card class="card" title="Stonework" seed="@juno/stonework" by="juno" description="A warm, chunky texture pack." :badges="['Installed', 'Textures']" dense />
        </Specimen>
        <Specimen name="Cover · Monogram" note="art drawn from a seed when a listing has none">
            <Cover v-for="s in ['@a/one', '@b/two', '@c/three', '@d/four']" :key="s" :seed="s" class="cover" />
            <Monogram name="@ash/kilns" /><Monogram name="@wren/iron-rails" :size="76" />
        </Specimen>
        <Specimen name="Shelf" note="one row of cards, the rest behind “see all”" wide>
            <Shelf title="Trending mods" more="See all" class="shelf">
                <Card v-for="n in 8" :key="n" :title="`Mod ${n}`" :seed="`@demo/mod-${n}`" by="demo" description="A line about it." />
            </Shelf>
        </Specimen>
        <Specimen name="CategoryTile" note="pixel icon, name, count">
            <div class="tiles">
                <CategoryTile v-for="(icon, id) in CATEGORY_ICONS" :key="id" :label="id" :rows="icon.rows" :palette="icon.palette" :count="12" />
            </div>
        </Specimen>
        <Specimen name="FacetList · FilterChips" note="pick any; the query in force, removable">
            <FacetList v-model="facets" title="Categories" class="facets" :options="[{ id: 'games', label: 'Games', count: 4 }, { id: 'machines', label: 'Machines', count: 6 }, { id: 'matter', label: 'Matter', count: 2 }]" />
            <FilterChips :chips="chips" @remove="(id) => (chips = chips.filter((c) => c.id !== id))" @clear="chips = []" />
        </Specimen>
        <Specimen name="PageHead" note="on the page, no card: mark, name, version, tabs along its foot" wide>
            <PageHead v-model:tab="tab" class="head" title="@ash/kilns" mark="@ash/kilns" badge="1.4.2" back="Mods"
                description="Fired clay, brick and glass from heat you have to build and feed."
                :tabs="[{ id: 'overview', label: 'Overview' }, { id: 'versions', label: 'Versions', count: 4 }]">
                <template #meta><span>by <b>ash</b></span><span><b>★ 164</b> stars</span></template>
            </PageHead>
        </Specimen>
        <Specimen name="Markdown" note="drawn from data, never HTML">
            <Markdown :source="README" />
        </Specimen>
        <Specimen name="FileTree" note="folders first, the chosen file lit">
            <FileTree v-model="file" :files="FILES" root="@ash/kilns" class="tree" />
        </Specimen>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(420px, 1fr)); gap: 28px; }
.framed { width: 100%; max-height: 900px; overflow-y: auto; background: var(--stone-0); }
.tall { height: 620px; }
.whole { width: 100%; height: 820px; }
.card { width: 220px; }
.cover { width: 160px; height: 90px; }
.shelf, .head { width: 100%; }
.tiles { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; width: 100%; }
.facets { width: 200px; }
.tree { width: 260px; }
</style>
