<template>
    <NuxtLink
        v-if="item.path"
        :to="item.path"
        class="nav-item"
        :class="{ active: route.path === item.path, folder: !!item.items, command: !!item.flavor }"
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
    <span v-else class="nav-item static" :class="{ folder: !!item.items }" :style="{ '--depth': depth }">
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

.nav-item {
    display: flex;
    align-items: center;
    gap: 0.35rem;
    font-size: 0.82rem;
    color: $text-secondary;
    white-space: nowrap;
    padding: 0.32rem 0.75rem;
    padding-left: calc(0.75rem + var(--depth) * #{$indent});
    text-decoration: none;

    &:hover {
        color: $text-primary;
        background: rgba($primary, 0.05);
    }

    &.active {
        color: $primary;
        background: rgba($primary, 0.08);
    }

    &.folder {
        font-weight: 600;
        color: $text-tertiary;
    }

    &.static:hover {
        background: transparent;
    }

    &.command {
        opacity: 0.8;

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
