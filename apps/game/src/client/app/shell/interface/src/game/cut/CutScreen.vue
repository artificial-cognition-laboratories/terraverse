<script setup lang="ts">
import { computed } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { Radial, type RadialItem } from "../../kit/index.ts"
import { useStore } from "../store.ts"

/**
 * What a cutting tool cuts (sprint 123): F on a block it cuts opens this. The forms round the
 * middle, each one's standard sizes beyond it; the middle is none (the tool digs whole blocks).
 * Taken, the cut is the server's to keep (server/game/cut), and F, Esc or a choice closes it.
 */
const bridge = useBridge()
const store = useStore()
const items = computed((): RadialItem[] => (store.ui.cut?.forms ?? []).map((f) => ({
    id: f.id, label: f.name, children: f.sizes.map((s) => ({ id: s.id, label: s.name })),
})))
const chosen = computed(() => {
    const c = store.ui.cut?.chosen
    return c ? { item: c.form, child: c.size } : null
})
function choose(payload: { form: string; size: string } | null) {
    void bridge.send("ui.send", { name: "cut.choose", payload: payload })
    void bridge.send("screen.close", {})
}
</script>

<template>
    <div class="cut-scene">
        <Radial :items="items" :chosen="chosen" none="Whole"
            @pick="(form, size) => size && choose({ form: form, size: size })" @none="choose(null)" />
    </div>
</template>

<style scoped>
/* round the crosshair: the world stays seen, and its middle is where they were looking */
.cut-scene { position: fixed; inset: 0; display: grid; place-items: center; }
</style>
