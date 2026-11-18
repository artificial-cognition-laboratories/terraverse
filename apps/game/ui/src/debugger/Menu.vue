<script setup lang="ts" generic="T extends string">
import { onUnmounted } from "vue"

/** One entry of a menu, or a rule between groups. */
export type MenuItem<Id extends string> =
    | { id: Id; label: string; icon?: string; checked?: boolean; shortcut?: string }
    | { rule: true }

/**
 * A devtools drop-down, hung from the button that opened it. Any press outside
 * it closes it (the press still lands where it was meant).
 */
const props = defineProps<{ items: readonly MenuItem<T>[]; checks?: boolean }>()
const emit = defineEmits<{ pick: [id: T]; close: [] }>()

const outside = (event: PointerEvent) => {
    if (!(event.target instanceof Element) || !event.target.closest(".dt-menu, .dt-opener")) emit("close")
}
document.addEventListener("pointerdown", outside, true)
onUnmounted(() => document.removeEventListener("pointerdown", outside, true))
</script>

<template>
    <ul class="dt-menu" role="menu">
        <template v-for="(item, i) in props.items" :key="i">
            <li v-if="'rule' in item" class="rule" role="separator" />
            <li v-else>
                <button role="menuitem" @click="emit('pick', item.id)">
                    <span v-if="checks" class="check">{{ item.checked ? "✓" : "" }}</span>
                    <svg v-if="item.icon" viewBox="0 0 16 16" aria-hidden="true"><path :d="item.icon" /></svg>
                    <span class="label">{{ item.label }}</span>
                    <span v-if="item.shortcut" class="shortcut">{{ item.shortcut }}</span>
                </button>
            </li>
        </template>
    </ul>
</template>

<style scoped>
.dt-menu {
    position: absolute; top: calc(100% + 2px); right: 0; z-index: 10; min-width: 180px; margin: 0; padding: 4px;
    list-style: none; background: var(--dt-menu); border: 1px solid var(--dt-menu-line); border-radius: 6px;
    box-shadow: 0 6px 20px rgb(0 0 0 / 0.45);
}
button {
    display: flex; align-items: center; gap: 8px; width: 100%; height: 28px; padding: 0 10px 0 8px;
    border: 0; border-radius: 4px; background: none; color: var(--dt-ink); font: inherit; font-size: 13px; text-align: left; cursor: default;
}
button:hover { background: var(--dt-menu-hover); }
.check { width: 12px; flex: none; color: var(--dt-ink); }
svg { width: 16px; height: 16px; flex: none; fill: none; stroke: currentColor; stroke-width: 1.3; stroke-linecap: round; stroke-linejoin: round; }
.label { flex: 1; white-space: nowrap; }
.shortcut { margin-left: 24px; color: var(--dt-dim); }
.rule { height: 1px; margin: 4px 6px; background: var(--dt-menu-line); }
</style>
