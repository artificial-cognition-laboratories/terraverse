<script setup lang="ts">
import { computed } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { Radial, type RadialItem } from "../../kit/index.ts"
import { useStore } from "../store.ts"

/**
 * What a shovel digs (sprint 124): F with one held opens this, wherever they look: its forms (a hole,
 * a trench, a pit, a flattened pad) round the middle, each one's sizes beyond it; the middle is the
 * first, as before any's chosen. Taken, the choice is the server's to keep (server/game/dig), and
 * F, Esc or a choice closes it.
 */
const bridge = useBridge()
const store = useStore()
const items = computed((): RadialItem[] => (store.ui.dig?.operations ?? []).map((o) => ({
    id: o.id, label: o.name, children: o.sizes.map((s) => ({ id: s.id, label: s.name })),
})))
const first = computed(() => store.ui.dig?.operations[0]?.name ?? "")
const chosen = computed(() => {
    const c = store.ui.dig?.chosen
    return c ? { item: c.operation, child: c.size } : null
})
function choose(payload: { operation: string; size: string } | null) {
    void bridge.send("ui.send", { name: "dig.choose", payload: payload })
    void bridge.send("screen.close", {})
}
</script>

<template>
    <div class="dig-scene">
        <Radial :items="items" :chosen="chosen" :none="first"
            @pick="(operation, size) => size && choose({ operation: operation, size: size })" @none="choose(null)" />
    </div>
</template>

<style scoped>
/* round the crosshair: the world stays seen, and its middle is where they were looking */
.dig-scene { position: fixed; inset: 0; display: grid; place-items: center; }
</style>
