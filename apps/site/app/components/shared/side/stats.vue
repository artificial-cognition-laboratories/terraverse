<script setup lang="ts">
import { ICONS, type IconId } from "~/config/icons"

/** A row of headline numbers in a side card, split by hairlines. */
defineProps<{ items: readonly { icon: IconId; value: string; label: string }[] }>()
</script>

<template>
    <div class="stats flush">
        <div v-for="item in items" :key="item.label">
            <home-pixel-icon :icon="ICONS[item.icon]" :size="14" />
            <strong>{{ item.value }}</strong>
            <span>{{ item.label }}</span>
        </div>
    </div>
</template>

<style scoped lang="scss">
.stats {
    display: grid;
    grid-auto-columns: 1fr;
    grid-auto-flow: column;
    padding: 0.9rem 0;

    div {
        display: grid;
        justify-items: center;
        gap: 0.25rem;

        & + div {
            border-left: 1px solid $glass-edge;
        }
    }

    strong {
        font-size: 0.95rem;
        color: $ink;
    }

    span {
        font-size: 0.62rem;
        color: $ink-faint;
    }
}
</style>
