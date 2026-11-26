<script setup lang="ts">
import type { Owner } from "~/registry"
import { format } from "~/lib/format"

/** An owner's page, laid out as a mod's: the header and their publishes, the side card beside them (on a phone, its numbers under the header). */
const props = defineProps<{ owner: Owner }>()
const stats = computed(() => [
    { icon: "mod" as const, value: format.count(props.owner.listings.length), label: "published" },
    { icon: "star" as const, value: format.count(props.owner.stars), label: "stars" },
    { icon: "download" as const, value: format.count(props.owner.installs), label: "installs" },
])
</script>

<template>
    <div class="owner">
        <main class="main">
            <registry-owner-header :owner="owner" />
            <div class="phone numbers"><shared-side-stats :items="stats" /></div>
            <section>
                <registry-row-head label="Overview" />
                <registry-cards :listings="owner.listings" :fill="owner.listings.length ? 0 : 1" />
            </section>
        </main>
        <registry-owner-sidebar class="side" :owner="owner" :stats="stats" />
    </div>
</template>

<style scoped lang="scss">
.owner {
    display: grid;
    grid-template-columns: minmax(0, 1fr) $side-width;
    gap: 2.5rem;
    align-items: start;
}

.main {
    display: grid;
    gap: 2rem;
    min-width: 0;
}

// the phone's own pieces: hidden only above tablet, so each keeps its own display (a strip stays a row)
@include desktop-up {
    .phone {
        display: none;
    }
}

@include tablet {
    .owner {
        grid-template-columns: minmax(0, 1fr);
    }

    .side {
        display: none;
    }

    .phone {
        @include panel;
    }
}
</style>
