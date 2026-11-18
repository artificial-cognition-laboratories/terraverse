<script setup lang="ts">
/**
 * Facts as a document, the way Axon Fleet and the settings lay them out:
 * titled sections, each with a line on what it is, then a definition list,
 * a row per fact (a name, maybe a note under it, and its value), hairlines
 * between. Read once and then glanced at, so prose, not tiles. Narrow, each
 * name sits above its value.
 */
export type Detail = { name: string; value: string; note?: string; mono?: boolean; dot?: "up" | "warn" | "off" }
export type DetailSection = { title: string; about?: string; rows: Detail[] }
defineProps<{ sections: readonly DetailSection[] }>()
</script>

<template>
    <div class="details">
        <section v-for="s in sections" :key="s.title">
            <div class="head">
                <h3>{{ s.title }}</h3>
                <p v-if="s.about">{{ s.about }}</p>
            </div>
            <dl>
                <div v-for="r in s.rows" :key="r.name" class="row">
                    <dt>{{ r.name }}<span v-if="r.note" class="note">{{ r.note }}</span></dt>
                    <dd :class="{ mono: r.mono }"><i v-if="r.dot" class="dot" :class="r.dot" />{{ r.value }}</dd>
                </div>
            </dl>
        </section>
    </div>
</template>

<style scoped>
.details { container-type: inline-size; padding: 0 0 16px; }
section { max-width: 780px; padding: 14px 14px 0; }
.head { margin-bottom: 8px; }
h3 { margin: 0 0 3px; font-size: 13px; font-weight: 600; color: var(--dt-ink); }
.head p { margin: 0; font-size: 11px; line-height: 1.5; color: var(--dt-dim); }
dl { display: grid; margin: 0; }
.row { display: grid; grid-template-columns: minmax(120px, 220px) minmax(0, 1fr); gap: 16px; align-items: baseline; padding: 8px 0; border-top: 1px solid var(--dt-line); }
dt { display: grid; gap: 2px; font-size: 11px; color: var(--dt-ink); }
.note { font-size: 10px; line-height: 1.4; color: var(--dt-faint); }
dd { display: flex; align-items: center; gap: 7px; min-width: 0; margin: 0; overflow-wrap: anywhere; font-size: 11px; color: var(--dt-ink); user-select: text; }
.mono { font-family: ui-monospace, "JetBrains Mono", monospace; font-variant-numeric: tabular-nums; }
.dot { flex: none; width: 6px; height: 6px; border-radius: 50%; background: var(--dt-faint); }
.dot.up { background: #5fd17a; }
.dot.warn { background: #e5a33b; }
@container (max-width: 380px) {
    .row { grid-template-columns: minmax(0, 1fr); gap: 3px; }
}
</style>
