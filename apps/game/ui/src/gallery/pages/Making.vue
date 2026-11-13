<script setup lang="ts">
import { onMounted } from "vue"
import { Button, Meter, Progress, Spinner, Stages } from "../../kit/index.ts"
import MakingScreen from "../../game/making/MakingScreen.vue"
import { cannedEntering, cannedLoading, useMock } from "../mock.ts"
import Specimen from "../Specimen.vue"

/** The real making screen against canned tf_craft offers and worktop: Make runs the bar and comes back. */
const mock = useMock()
// a world partway through its rivers
const loading = cannedLoading(3)
if (!loading) throw new Error("the canned world is made by 3 s")
// a world being entered, its media and textures coming in
const entering = cannedEntering(3.8)
if (!entering) throw new Error("the canned world is entered by 5 s")
onMounted(() => {
    mock.emit({ kind: "mode", mode: "game" })
    mock.emit({ kind: "state", section: "hotbar", value: { size: 9, selected: 0 } })
    mock.stock({ 0: { item: "tf_world:stone", count: 6, wear: 0 }, 1: { item: "tf_world:dirt", count: 3, wear: 0 } })
    mock.send("ui.send", { name: "making.hand", payload: {} }).catch(console.error)
})
</script>

<template>
    <div class="page">
        <Specimen name="Progress" note="fills over its seconds, one transition; or as far as measured">
            <Progress :seconds="4" label="Knap: flint flake" class="bar" />
            <Progress :value="0.42" label="Making the world · 42%" class="bar" />
        </Specimen>
        <Specimen name="Stages" note="work in stages: under way, done in its time, or kept and never run">
            <Stages :stages="loading.stages" class="bar" />
        </Specimen>
        <Specimen name="Spinner" note="under way for no time anyone can say; stepped, never a smooth spin">
            <Spinner />
        </Specimen>
        <Specimen name="Button busy" note="what was pressed, under way: a spinner, not pressable, not faded">
            <div class="row">
                <Progress :value="entering.share" label="Preparing textures and shaders" class="entering-bar" />
                <Button variant="primary" busy>Play</Button>
            </div>
            <Stages :stages="entering.stages" class="bar" />
        </Specimen>
        <Specimen name="Meter" note="an amount against marks: what's carried, toned past them">
            <div class="meters">
                <Meter :value="12" :max="150" :marks="[25, 50]" label="Carrying 12 kg · light" class="bar" />
                <Meter :value="38" :max="150" :marks="[25, 50]" label="Carrying 38 kg · slowed to 84%" tone="warn" class="bar" />
                <Meter :value="120" :max="150" :marks="[25, 50]" label="Carrying 120 kg · slowed to 34%" tone="danger" class="bar" />
            </div>
        </Specimen>
        <div class="world">
            <div class="sky" />
            <MakingScreen />
        </div>
    </div>
</template>

<style scoped>
.page { display: flex; flex-direction: column; gap: 20px; height: 100%; }
.bar { width: 320px; }
.row { display: flex; gap: 8px; margin-bottom: 8px; }
.entering-bar { width: 280px; height: 44px; }
.entering-bar :deep(.label) { line-height: 40px; }
.meters { display: flex; flex-direction: column; gap: 8px; }
.world { position: relative; flex: 1; min-height: 560px; overflow: hidden; border: var(--b) solid var(--edge); transform: translateZ(0); }
.sky { position: absolute; inset: 0; background: linear-gradient(180deg, #8ab6f0, #c4dcf5 60%, #5b8f3a 60%, #4c7a31); }
</style>
