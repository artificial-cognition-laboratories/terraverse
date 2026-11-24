<template>
    <NuxtLink
        v-if="item.path"
        :to="item.path"
        class="nav-item"
        :class="{ active: route.path === item.path, folder: !!item.items, file: !!item.icon, command: !!item.flavor, muted: !!item.muted }"
        :style="{ '--depth': depth }"
    >
        <Icon v-if="item.icon" :name="item.icon" class="nav-icon" />
        <span v-if="command" class="nav-title">
            <span class="cmd-program">{{ command.program }}</span>
            <template v-for="(arg, i) in command.args" :key="i">
                {{ " " }}<span :class="arg.startsWith('<') ? 'cmd-placeholder' : 'cmd-arg'">{{ arg }}</span>
            </template>
        </span>
        <span v-else class="nav-title">{{ item.title }}</span>
    </NuxtLink>
    <span v-else class="nav-item static" :class="{ folder: !!item.items, file: !!item.icon }" :style="{ '--depth': depth }">
        <Icon v-if="item.icon" :name="item.icon" class="nav-icon" />
        <span class="nav-title">{{ item.title }}</span>
    </span>
    <ul v-if="item.items?.length" class="nav-children">
        <li v-for="child in item.items" :key="child.path ?? child.title">
            <docs-sidebar-item :item="child" :depth="depth + 1" />
        </li>
    </ul>
</template>

<script setup lang="ts">
import type { DocsNavItem } from "~/config/docs-nav"

const props = defineProps<{ item: DocsNavItem; depth: number }>()
const route = useRoute()

// A command is its program, then its words; a `<placeholder>` is coloured apart from literal ones
const command = computed(() => {
    if (props.item.flavor !== "bash") return undefined
    const [program, ...args] = props.item.title.split(" ")
    return { program, args }
})
</script>

<style scoped lang="scss">
$indent: 0.85rem;

// Lit as the side card's own choices are: a tint, and an edge of the brand
.nav-item {
    display: flex;
    align-items: center;
    gap: 0.45rem;
    margin-right: 0.5rem; // clear of the tree's scrollbar
    padding: 0.38rem 0.65rem;
    padding-left: calc(0.65rem + var(--depth) * #{$indent});
    font-family: $font-sans;
    font-size: 0.84rem;
    color: $ink-dim;
    white-space: nowrap;
    text-decoration: none;
    box-shadow: inset 2px 0 0 transparent;

    &:hover {
        color: $ink;
    }

    &.active {
        color: $ink;
        background: rgba($primary-ink, 0.07);
        box-shadow: inset 2px 0 0 $primary-ink;
    }

    // Files, folders and commands are literal: they keep the code face
    &.file,
    &.command {
        font-family: $font-mono;
        font-size: 0.78rem;
    }

    &.folder {
        color: $ink;
    }

    &.muted:not(.active) {
        opacity: 0.5;
    }

    &.static:hover {
        color: $ink-dim;
    }

    &.command {
        opacity: 0.85;

        &:hover,
        &.active {
            opacity: 1;
        }
    }
}

.nav-title {
    overflow: hidden;
    text-overflow: ellipsis;
}

// The dark-plus bash colours the docs' code blocks use
.cmd-program {
    color: #dcdcaa;
}

.cmd-arg {
    color: #ce9178;
}

.cmd-placeholder {
    color: #d4d4d4;
}

.nav-children {
    list-style: none;
    padding: 0;
    margin: 0;
}

.nav-icon {
    flex-shrink: 0;
    width: 1rem;
    height: 1rem;
}
</style>
