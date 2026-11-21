<script setup lang="ts">
/**
 * Every code block in markdown (the docs, a mod's readme): the landing's window,
 * its label on the left (the file's name when the fence gives one, else the
 * language), its dots on the right. Pointed at, the dots give way to "copy".
 * Shiki's highlighting arrives as the slot.
 */
defineOptions({ inheritAttrs: false })
const props = withDefaults(
    defineProps<{
        code?: string
        language?: string | null
        filename?: string | null
        highlights?: number[]
        meta?: string | null
        class?: string | null
    }>(),
    {
        code: "",
        language: null,
        filename: null,
        highlights: () => [],
        meta: null,
        class: null,
    },
)
const copy = useCopy()
</script>

<template>
    <div class="prose-window">
        <home-window dots="right">
            <template #bar>
                <span class="label">{{ props.filename ?? props.language }}</span>
                <button type="button" class="copy" @click="copy.copy(props.code)">{{ copy.label.value }}</button>
            </template>
            <pre :class="props.class"><slot /></pre>
        </home-window>
    </div>
</template>

<style scoped lang="scss">
.prose-window {
    margin: 1.75rem 0;
}

.label {
    flex: 1;
    font-family: $font-mono;
    font-size: 0.72rem;
    color: $ink-faint;
}

// Sits over the dots, hidden until the block is pointed at: it takes no room of its own
.copy {
    position: absolute;
    top: 50%;
    right: 1rem;
    padding: 0;
    font-family: $font-mono;
    font-size: 0.68rem;
    color: $ink-faint;
    background: none;
    border: none;
    cursor: pointer;
    opacity: 0;
    transform: translateY(-50%);
    transition: opacity 0.15s;

    &:hover {
        color: $ink;
    }
}

.prose-window :deep(.dots) {
    transition: opacity 0.15s;
}

.prose-window:hover,
.prose-window:focus-within {
    .copy {
        opacity: 1;
    }

    :deep(.dots) {
        opacity: 0;
    }
}

pre {
    margin: 0;
    padding: 0 1.25rem 1.1rem;
    overflow-x: auto;
    background: none !important; // Shiki writes its theme's background inline
    scrollbar-width: thin;
    scrollbar-color: $night-4 transparent;

    // Each line is its own block, and the \n Shiki leaves between them would be
    // a second, empty line: the code itself has no height, its lines have it
    :deep(code) {
        display: block;
        font-family: $font-mono;
        font-size: 0.8rem;
        line-height: 1.65;
    }

    :deep(code:has(.line)) {
        line-height: 0;
    }

    :deep(code .line) {
        display: block;
        min-height: 1.65em;
        line-height: 1.65;
    }
}
</style>
