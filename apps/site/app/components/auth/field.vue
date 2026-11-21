<script setup lang="ts">
/** One labelled input on an account screen: a well, lit when it has the focus. */
defineProps<{ label: string; type?: string; autocomplete?: string; placeholder?: string }>()
const value = defineModel<string>({ required: true })
</script>

<template>
    <label class="field">
        <span class="label">{{ label }}</span>
        <input v-model="value" :type="type ?? 'text'" :autocomplete="autocomplete" :placeholder="placeholder" spellcheck="false" />
    </label>
</template>

<style scoped lang="scss">
.field {
    display: grid;
    gap: 0.45rem;
}

.label {
    font-family: $font-display;
    font-size: 0.58rem;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    color: $ink-faint;
}

input {
    @include well;
    width: 100%;
    min-width: 0;
    height: 2.8rem;
    padding: 0 0.9rem;
    font-family: $font-sans;
    font-size: 0.95rem;
    color: $ink;
    outline: none;

    &:focus {
        border-color: rgba($primary-ink, 0.55);
    }

    &::placeholder {
        color: $ink-faint;
    }
}
</style>
