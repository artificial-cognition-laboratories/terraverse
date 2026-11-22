<script setup lang="ts">
import { useRegistry } from "~/registry"
import { format } from "~/lib/format"

definePageMeta({ layout: "landing" })

// An owner's page: /registry/@scope
const route = useRoute()
const scope = computed(() => String(route.params.scope))
const reg = useRegistry()
const { data: owner } = await useAsyncData(() => `owner:${scope.value}`, () => reg.owner(scope.value))
if (!scope.value.startsWith("@") || !owner.value) {
    throw createError({ statusCode: 404, statusMessage: `Nobody called ${scope.value} has published yet.`, fatal: true })
}
useHead({ title: () => scope.value })
</script>

<template>
    <registry-shell>
        <template v-if="owner">
            <header class="owner">
                <registry-monogram :title="owner.scope.slice(1)" size="lg" />
                <div>
                    <div class="line">
                        <h1>{{ owner.scope }}</h1>
                        <span v-if="owner.official" class="badge">Official</span>
                    </div>
                    <p>{{ owner.listings.length }} published · ★ {{ format.count(owner.stars) }} · ↓ {{ format.count(owner.installs) }}</p>
                </div>
            </header>
            <registry-row-head label="Published" />
            <registry-cards :listings="owner.listings" />
        </template>
    </registry-shell>
</template>

<style scoped lang="scss">
.owner {
    @include on-sky;
    display: flex;
    gap: 1.25rem;
    align-items: center;
}

.line {
    display: flex;
    gap: 0.75rem;
    align-items: center;
}

h1 {
    margin: 0;
    font-family: $font-display;
    font-weight: 400;
    font-size: 1.8rem;
    color: $ink;
}

.badge {
    padding: 0.2rem 0.45rem;
    font-family: $font-display;
    font-size: 0.55rem;
    text-transform: uppercase;
    color: $primary-ink;
    border: 1px solid rgba($primary-ink, 0.55);
}

p {
    margin: 0.4rem 0 0;
    font-size: 0.8rem;
    color: $ink-faint;
}
</style>
