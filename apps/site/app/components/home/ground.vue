<script setup lang="ts">
import { paint } from "~/lib/paint"
import { PX, sky } from "~/lib/sky"

/**
 * Where the landing comes to land: a far range that rises into place as you
 * arrive, the near ridge, then rock down to bedrock, where the slot (the footer) sits.
 */
const props = defineProps<{ seed: string }>()

const HEIGHT = 110 // art pixels

const root = ref<HTMLElement | null>(null)
const far = ref<HTMLElement | null>(null)
const art = reactive({ far: "", near: "", width: 0 })

function draw() {
    const width = Math.ceil((root.value?.clientWidth ?? 0) / PX)
    if (!width || width === art.width) return
    const ground = sky.ground(props.seed, width, HEIGHT)
    art.far = paint(ground.far)
    art.near = paint(ground.near)
    art.width = width
}

onMounted(() => {
    draw()
    window.addEventListener("resize", draw)
})
onBeforeUnmount(() => window.removeEventListener("resize", draw))

// The far range sits low until the ground comes into view, then settles behind the ridge
useScrollFrame(() => {
    if (!root.value || !far.value) return
    const below = root.value.getBoundingClientRect().top - window.innerHeight * 0.45
    far.value.style.transform = `translate3d(0, ${Math.min(90, Math.max(0, below * 0.18))}px, 0)`
})

const size = computed(() => `${art.width * PX}px ${HEIGHT * PX}px`)
</script>

<template>
    <div ref="root" class="ground" :class="{ drawn: art.near }" :style="{ height: `${HEIGHT * PX}px` }">
        <div ref="far" class="layer" :style="art.far ? { backgroundImage: `url(${art.far})`, backgroundSize: size } : {}" aria-hidden="true" />
        <div class="layer" :style="art.near ? { backgroundImage: `url(${art.near})`, backgroundSize: size } : {}" aria-hidden="true" />
        <div class="on">
            <slot />
        </div>
    </div>
</template>

<style scoped lang="scss">
.ground {
    position: relative;
    overflow: hidden;
}

.layer {
    position: absolute;
    inset: 0;
    background-repeat: no-repeat;
    background-position: left top;
    image-rendering: pixelated;
    opacity: 0;
    transition: opacity 0.6s;
    will-change: transform;

    .drawn & {
        opacity: 1;
    }
}

.on {
    position: absolute;
    left: 0;
    right: 0;
    bottom: 0;
}
</style>
