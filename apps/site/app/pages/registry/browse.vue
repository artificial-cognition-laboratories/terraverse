<script setup lang="ts">
import { query, type View } from "~/registry/query"
import { SORT_LABELS, keys, known, useRegistry, type Search } from "~/registry"
import { format } from "~/lib/format"

definePageMeta({ layout: "landing" })
useHead({ title: "Browse the registry" })

const route = useRoute()
const reg = useRegistry()
const state = computed(() => query.read(route.query))
const { data } = await useAsyncData(
    () => `registry-browse:${JSON.stringify(state.value.search)}`,
    async () => ({ results: await reg.search(state.value.search), categories: await reg.categories(), totals: await reg.totals() }),
    { watch: [state] },
)
const pages = computed(() => (data.value ? Math.ceil(data.value.results.total / data.value.results.pageSize) : 1))

// Every change is a new URL: back and forward step through searches
function go(change: Partial<Search>, view: View = state.value.view) {
    // a new filter starts again at the first page; only paging keeps its place
    const next = { ...state.value.search, page: 1, ...change }
    return navigateTo({ query: query.write(next, view) }, { replace: !("page" in change) })
}

const SORTS = keys(SORT_LABELS)

function sortBy(event: Event) {
    const value = event.target instanceof HTMLSelectElement ? event.target.value : ""
    if (known(SORT_LABELS, value)) go({ sort: value })
}
</script>

<template>
    <registry-shell>
        <div class="layout">
            <registry-filters
                v-if="data"
                :search="state.search"
                :categories="data.categories"
                :totals="data.totals"
                @change="go"
            />

            <div class="results">
                <header>
                    <div>
                        <h1>{{ state.search.query ? `“${state.search.query}”` : "Browse" }}</h1>
                        <p v-if="data">{{ data.results.total }} {{ data.results.total === 1 ? "result" : "results" }}</p>
                    </div>
                    <div class="controls">
                        <label class="sort">
                            <span>Sort</span>
                            <select :value="state.search.sort" @change="sortBy">
                                <option v-for="s in SORTS" :key="s" :value="s">{{ SORT_LABELS[s] }}</option>
                            </select>
                        </label>
                        <div class="views" role="group" aria-label="Layout">
                            <button type="button" :class="{ on: state.view === 'grid' }" aria-label="Grid" @click="go({}, 'grid')">▦</button>
                            <button type="button" :class="{ on: state.view === 'list' }" aria-label="List" @click="go({}, 'list')">≡</button>
                        </div>
                    </div>
                </header>

                <template v-if="data && data.results.listings.length">
                    <registry-cards v-if="state.view === 'grid'" :listings="data.results.listings" class="grid" />
                    <div v-else class="list">
                        <registry-row v-for="l in data.results.listings" :key="l.name" :listing="l" />
                    </div>
                    <footer>
                        <span>Showing {{ (data.results.page - 1) * data.results.pageSize + 1 }}–{{ (data.results.page - 1) * data.results.pageSize + data.results.listings.length }} of {{ format.count(data.results.total) }}</span>
                        <registry-pager :page="data.results.page" :pages="pages" @go="go({ page: $event })" />
                    </footer>
                </template>

                <div v-else-if="data" class="empty">
                    <h2>Nothing matches that yet.</h2>
                    <p>Try fewer filters, or make the thing you were looking for.</p>
                    <home-button to="/docs/mods">Write a mod »</home-button>
                </div>
            </div>
        </div>
    </registry-shell>
</template>

<style scoped lang="scss">
.layout {
    display: grid;
    grid-template-columns: $side-width minmax(0, 1fr);
    gap: 3rem;
    align-items: start;
}

header {
    @include on-sky;
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 1.5rem;
    margin-bottom: 1.5rem;
    padding-bottom: 1rem;
    border-bottom: 1px solid $glass-edge;

    h1 {
        margin: 0;
        font-family: $font-sans;
        font-size: 1.6rem;
        color: $ink;
    }

    p {
        margin: 0.3rem 0 0;
        font-size: 0.75rem;
        color: $ink-faint;
    }
}

.controls {
    display: flex;
    gap: 0.75rem;
    align-items: center;
}

.sort {
    display: flex;
    gap: 0.6rem;
    align-items: center;
    font-size: 0.7rem;
    color: $ink-faint;

    select {
        @include panel;
        height: 2.1rem;
        padding: 0 0.6rem;
        font-family: $font-mono;
        font-size: 0.75rem;
        color: $ink;
        cursor: pointer;

        option {
            background: $night-0;
        }
    }
}

.views {
    display: flex;

    button {
        @include panel;
        width: 2.1rem;
        height: 2.1rem;
        font-size: 0.95rem;
        color: $ink-faint;
        cursor: pointer;

        & + button {
            border-left: none;
        }

        &.on {
            color: $ink;
            background-color: rgba($primary, 0.3);
            border-color: rgba($primary-ink, 0.55);
        }
    }
}

.grid {
    grid-template-columns: repeat(auto-fill, minmax(15rem, 1fr));
}

.list {
    display: grid;
    gap: 0.75rem;
}

footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-top: 2rem;
    font-size: 0.72rem;
    color: $ink-faint;
}

.empty {
    @include panel;
    padding: 3rem;
    text-align: center;

    h2 {
        margin: 0 0 0.5rem;
        font-family: $font-sans;
        color: $ink;
    }

    p {
        margin: 0 0 1.5rem;
        color: $ink-dim;
    }
}

@include tablet {
    .layout {
        grid-template-columns: minmax(0, 1fr);
    }

    header {
        flex-direction: column;
        align-items: flex-start;
    }
}
</style>
