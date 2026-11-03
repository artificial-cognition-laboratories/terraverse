<script setup lang="ts">
import { provide, watch } from "vue"
import { useBridge } from "../../bridge/use.ts"
import { WorldDraft } from "../../menu/create/draft.ts"
import BodyStrip from "../../system/BodyStrip.vue"
import GameTab from "../../menu/create/GameTab.vue"
import DynamicsTab from "../../menu/create/DynamicsTab.vue"
import Viewer from "../../menu/create/Viewer.vue"
import WorldTab from "../../menu/create/WorldTab.vue"
import { LOOKS, Looks } from "../../menu/create/looks.ts"
import { SYSTEM_PREVIEW, SystemPreview } from "../../system/preview.ts"
import Specimen from "../Specimen.vue"
import { CANNED_HOME } from "../mock.ts"

const draft = WorldDraft({ home: CANNED_HOME })
const bridge = useBridge()
const system = SystemPreview({ bridge })
provide(SYSTEM_PREVIEW, system)
watch(() => [draft.system, draft.config.star], () => system.request(draft.system, draft.config), { deep: true, immediate: true })
const looks = Looks({ bridge, draft, cards: system })
provide(LOOKS, looks)
</script>

<template>
    <div class="grid">
        <Specimen name="Viewer" note="interactive 3D system · body list · globe ↔ map ↔ system">
            <div class="box"><BodyStrip :system="draft.system" :star="draft.config.star" :selected="draft.system.selected" :image="looks.image"
                @pick="(id) => { draft.system.selected = id }" /><Viewer :draft="draft" :frame="{ left: 0, right: 0 }" /></div>
        </Specimen>
        <div class="col">
            <Specimen name="Game tab"><div class="col"><GameTab :draft="draft" /></div></Specimen>
            <Specimen name="World tab"><div class="col"><WorldTab :draft="draft" /></div></Specimen>
            <Specimen name="Dynamics tab"><div class="col"><DynamicsTab :draft="draft" /></div></Specimen>
        </div>
    </div>
</template>

<style scoped>
.grid { display: grid; grid-template-columns: minmax(0, 1fr) 440px; gap: 28px; align-items: start; }
.box { display: flex; flex-direction: column; width: 100%; height: 680px; }
.box :deep(.viewer) { flex: 1; }
.col { display: flex; flex-direction: column; gap: 22px; width: 100%; }
</style>
