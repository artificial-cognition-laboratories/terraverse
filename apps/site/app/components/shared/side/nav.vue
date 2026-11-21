<script setup lang="ts" generic="Id extends string">
import { ICONS, type IconId } from "~/config/icons"

/** Stacked choices in a side card: a glyph, a label, a count; the chosen one lit. */
defineProps<{ items: readonly { id: Id; label: string; icon: IconId; count?: number }[]; active: Id }>()
const emit = defineEmits<{ pick: [id: Id] }>()
</script>

<template>
    <nav class="nav flush">
        <button
            v-for="item in items"
            :key="item.id"
            type="button"
            :class="{ on: active === item.id }"
            :aria-pressed="active === item.id"
            @click="emit('pick', item.id)"
        >
            <home-pixel-icon :icon="ICONS[item.icon]" :size="16" />
            <span class="label">{{ item.label }}</span>
            <span v-if="item.count !== undefined" class="count">{{ item.count }}</span>
        </button>
    </nav>
</template>

<style scoped lang="scss">
.nav {
    display: grid;
    gap: 0.15rem;
    padding: 0.5rem;

    button {
        display: flex;
        gap: 0.75rem;
        align-items: center;
        padding: 0.6rem 0.65rem;
        font-family: $font-display;
        font-size: 0.66rem;
        text-transform: uppercase;
        text-align: left;
        color: $ink-dim;
        background: none;
        border: none;
        box-shadow: inset 2px 0 0 transparent;
        cursor: pointer;

        &:hover {
            color: $ink;
        }

        &.on {
            color: $ink;
            background: rgba($primary-ink, 0.07);
            box-shadow: inset 2px 0 0 $primary-ink;
        }
    }

    .label {
        flex: 1;
    }

    .count {
        font-family: $font-mono;
        font-size: 0.68rem;
        color: $ink-faint;
    }
}
</style>
