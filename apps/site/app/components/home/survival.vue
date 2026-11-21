<script setup lang="ts">
import { survival } from "~/config/home"
import { SYSTEM_ICONS } from "~/config/icons"
</script>

<template>
    <home-band>
        <div class="split">
            <home-heading :kicker="survival.kicker" :title="survival.title" :lede="survival.lede" />
            <ul class="points">
                <li v-for="p in survival.points" :key="p.title">
                    <span class="icon"><home-pixel-icon :icon="SYSTEM_ICONS[p.icon]" :size="36" /></span>
                    <div>
                        <h3>{{ p.title }}</h3>
                        <p>{{ p.text }}</p>
                    </div>
                </li>
            </ul>
        </div>
    </home-band>
</template>

<style scoped lang="scss">
.split {
    display: grid;
    grid-template-columns: minmax(0, 5fr) minmax(0, 6fr);
    gap: 5rem;
    align-items: center;

    // the heading sits level with the list, not above a gap
    :deep(.heading) {
        margin-bottom: 0;
    }
}

.points {
    display: grid;
    gap: 1rem;
    margin: 0;
    padding: 0;
    list-style: none;
}

li {
    @include panel;
    display: flex;
    gap: 1.5rem;
    align-items: center;
    padding: 1.5rem 1.75rem;
}

.icon {
    flex: none;
    display: grid;
    place-items: center;
    width: 4rem;
    height: 4rem;
    background: $stone-0;
    border: 1px solid $stone-3;
}

h3 {
    margin: 0 0 0.4rem;
    font-family: $font-sans;
    font-size: 1.05rem;
    color: $ink;
}

p {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.92rem;
    line-height: 1.55;
    color: $ink-dim;
}

@include tablet {
    .split {
        grid-template-columns: minmax(0, 1fr);
        gap: 2.5rem;
    }
}

@include mobile {
    li {
        padding: 1.25rem;
        gap: 1rem;
    }
}
</style>
