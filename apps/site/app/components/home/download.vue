<template>
    <div id="download" class="download">
        <div class="tabs" role="tablist">
            <button
                v-for="p in platforms"
                :key="p.id"
                role="tab"
                class="tab"
                :aria-selected="p.id === shown.id"
                :class="{ active: p.id === shown.id }"
                @click="chosen = p.id"
            >
                <Icon :name="p.icon" />
                {{ p.label }}
                <span v-if="p.id === detected" class="yours">yours</span>
            </button>
        </div>

        <div class="panel">
            <p v-if="shown.notReady" class="not-ready">{{ shown.notReady }}</p>
            <template v-else>
                <div v-if="shown.command" class="command">
                    <span class="prompt">$</span>
                    <code>{{ shown.command }}</code>
                    <button class="copy" @click="copy(shown.command)">{{ copied }}</button>
                </div>
                <div class="files">
                    <span v-if="shown.command" class="or">or download</span>
                    <a v-for="d in shown.downloads" :key="d.href" :href="d.href" class="file">
                        <span class="file-label">{{ d.label }}</span>
                        <span class="file-detail">{{ d.detail }}</span>
                    </a>
                </div>
            </template>
        </div>
    </div>
</template>

<script setup lang="ts">
import { platforms, type Platform, type PlatformId } from "~/config/downloads"

const { detected } = usePlatform()

// The visitor's pick wins; then their OS; then the first platform, which is
// what the prerendered page shows before anything is known
const chosen = ref<PlatformId | null>(null)
const shown = computed<Platform>(() => {
    const id = chosen.value ?? detected.value
    const found = platforms.find(p => p.id === id) ?? platforms[0]
    if (!found) throw new Error("downloads: no platforms")
    return found
})

const copied = ref("copy")
async function copy(text: string) {
    try {
        await navigator.clipboard.writeText(text)
        copied.value = "copied"
    } catch {
        copied.value = "select it"
    }
    setTimeout(() => (copied.value = "copy"), 1600)
}
</script>

<style scoped lang="scss">
.download {
    max-width: 38rem;
    background: rgba($bg-space, 0.82);
    border: 1px solid $border-mid;
}

.tabs {
    display: flex;
    border-bottom: 1px solid $border-soft;
}

.tab {
    display: flex;
    align-items: center;
    gap: 0.45rem;
    padding: 0.7rem 1rem;
    font-size: 0.8rem;
    color: $text-tertiary;
    background: transparent;
    border: none;
    border-bottom: 1px solid transparent;
    margin-bottom: -1px;
    cursor: pointer;
    white-space: nowrap;

    &:hover {
        color: $text-secondary;
    }

    &.active {
        color: $primary;
        border-bottom-color: $primary;
    }
}

.yours {
    font-size: 0.6rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: $text-tertiary;
    border: 1px solid $border-soft;
    padding: 0.05rem 0.3rem;
}

.panel {
    padding: 1rem;
}

.command {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    background: $surface-solid;
    border: 1px solid $border-soft;
    padding: 0.6rem 0.75rem;

    .prompt {
        color: $primary;
    }

    code {
        flex: 1;
        min-width: 0;
        font-size: 0.8rem;
        color: $text-primary;
        overflow-x: auto;
        white-space: nowrap;
        scrollbar-width: none;
    }
}

.copy {
    flex: none;
    font-size: 0.72rem;
    color: $text-secondary;
    background: transparent;
    border: 1px solid $border-mid;
    padding: 0.25rem 0.6rem;
    cursor: pointer;

    &:hover {
        color: $primary;
        border-color: $border-strong;
    }
}

.files {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.6rem;

    .command + & {
        margin-top: 0.85rem;
    }
}

.or {
    font-size: 0.72rem;
    color: $text-tertiary;
}

.file {
    display: flex;
    align-items: baseline;
    gap: 0.5rem;
    padding: 0.45rem 0.8rem;
    border: 1px solid $border-mid;
    text-decoration: none;

    &:hover {
        border-color: $primary;
        background: rgba($primary, 0.06);
    }
}

.file-label {
    color: $primary;
    font-size: 0.82rem;
}

.file-detail {
    color: $text-tertiary;
    font-size: 0.7rem;
}

.not-ready {
    margin: 0;
    font-size: 0.82rem;
    color: $text-secondary;
}

@include mobile {
    .tab {
        padding: 0.7rem 0.7rem;
    }

    .yours {
        display: none;
    }
}
</style>
