<script setup lang="ts">
import { ICONS } from "~/config/icons"

/**
 * The glass search box in a subnav. It only holds the words; what a search does
 * (open Browse, list docs pages) is its owner's, through `submit` and the slot
 * under the box (a list of results).
 */
const props = defineProps<{ placeholder: string; label: string }>()
const words = defineModel<string>({ required: true })
const emit = defineEmits<{ submit: []; keydown: [event: KeyboardEvent]; focus: [] }>()
const input = ref<HTMLInputElement | null>(null)

defineExpose({ focus: () => input.value?.focus(), blur: () => input.value?.blur() })
</script>

<template>
    <form class="search" role="search" @submit.prevent="emit('submit')">
        <home-pixel-icon :icon="ICONS.search" :size="14" class="glass" />
        <input
            ref="input"
            v-model="words"
            type="search"
            :placeholder="props.placeholder"
            :aria-label="props.label"
            autocomplete="off"
            @keydown="emit('keydown', $event)"
            @focus="emit('focus')"
        />
        <slot name="hint" />
        <slot />
    </form>
</template>

<style scoped lang="scss">
.search {
    @include panel;
    position: relative;
    display: flex;
    align-items: center;
    gap: 0.6rem;
    width: min(26rem, 50%);
    height: 2.2rem;
    padding: 0 0.8rem;

    &:focus-within {
        border-color: rgba($primary-ink, 0.55);
    }

    .glass {
        flex: none;
        color: $ink-faint;
    }

    input {
        flex: 1;
        min-width: 0;
        font-family: $font-mono;
        font-size: 0.75rem;
        color: $ink;
        background: none;
        border: none;
        outline: none;

        &::placeholder {
            color: $ink-faint;
        }

        &::-webkit-search-cancel-button {
            display: none;
        }
    }
}

@include mobile {
    .search {
        width: auto;
        flex: 1;
    }
}
</style>
