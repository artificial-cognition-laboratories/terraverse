<script setup lang="ts">
import { onMounted } from "vue"
import InventoryScreen from "../../game/inventory/InventoryScreen.vue"
import { useMock } from "../mock.ts"

/** The real inventory screen against the mock engine's inventory rules. */
const mock = useMock()

onMounted(() => {
    mock.emit({ kind: "mode", mode: "game" })
    mock.emit({ kind: "state", section: "hotbar", value: { size: 9, selected: 0 } })
    mock.stock({
        0: { item: "core:grass", count: 8, wear: 0 },
        1: { item: "core:dirt", count: 3, wear: 0 },
        2: { item: "core:stone", count: 64, wear: 0 },
        3: { item: "core:sand", count: 64, wear: 0 },
        5: { item: "tf_tools:pick", count: 1, wear: 21000 },
        12: { item: "core:stone", count: 7, wear: 0 },
    })
})
</script>

<template>
    <div class="world">
        <div class="sky" />
        <InventoryScreen />
    </div>
</template>

<style scoped>
.world { position: relative; height: 100%; min-height: 640px; overflow: hidden; border: var(--b) solid var(--edge); transform: translateZ(0); }
.sky { position: absolute; inset: 0; background: linear-gradient(180deg, #8ab6f0, #c4dcf5 60%, #5b8f3a 60%, #4c7a31); }
</style>
