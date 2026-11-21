<script setup lang="ts">
import { paint } from "~/lib/paint"
import { PX, sky } from "~/lib/sky"

/**
 * The night the landing hangs in, behind every section: one flat dark, two
 * fields of stars, and on the landing the sun and the moon (`scene`). Each moves at its own depth as the
 * page scrolls (`speed`: 1 is with the page, 0 is fixed to the window).
 */
const props = withDefaults(defineProps<{ seed: string; scene?: "landing" | "stars" }>(), { scene: "landing" })

// Each field is a tile that repeats. The far one is dim enough to repeat unnoticed;
// the near one, with its glints, is wider than a screen so it never visibly does.
const FIELDS = [
    { id: "far", seed: "far", count: 110, glints: 0, speed: 0.05, tile: { width: 160, height: 120 } },
    { id: "near", seed: "near", count: 60, glints: 6, speed: 0.22, tile: { width: 420, height: 260 } },
] as const
const BODIES = { sun: { speed: 0.12 }, moon: { speed: 0.32 } }

const fields = ref<HTMLElement[]>([])
const sun = ref<HTMLElement | null>(null)
const moon = ref<HTMLElement | null>(null)
const images = reactive<{ stars: string[]; sun: string; moon: string; size: { sun: number; moon: number } }>({
    stars: [],
    sun: "",
    moon: "",
    size: { sun: 0, moon: 0 },
})
const drawn = ref(false)

onMounted(() => {
    images.stars = FIELDS.map(f => paint(sky.stars(`${props.seed}:${f.seed}`, f.tile.width, f.tile.height, f.count, f.glints)))
    if (props.scene === "stars") {
        drawn.value = true
        return
    }
    const sunArt = sky.sun(7)
    const moonArt = sky.moon(`${props.seed}:moon`, 13, [0.75, -0.45, 0.5])
    images.sun = paint(sunArt)
    images.moon = paint(moonArt)
    images.size = { sun: sunArt.width * PX, moon: moonArt.width * PX }
    drawn.value = true
})

useScrollFrame(scroll => {
    FIELDS.forEach((f, i) => {
        const el = fields.value[i]
        if (el) el.style.transform = `translate3d(0, ${-((scroll * f.speed) % (f.tile.height * PX))}px, 0)`
    })
    // a body placed on the page rises with it; held back, it seems further away
    if (sun.value) sun.value.style.transform = `translate3d(0, ${scroll * (1 - BODIES.sun.speed)}px, 0)`
    if (moon.value) moon.value.style.transform = `translate3d(0, ${scroll * (1 - BODIES.moon.speed)}px, 0)`
})
</script>

<template>
    <div
        class="sky"
        :class="{ drawn }"
        aria-hidden="true"
    >
        <div
            v-for="(f, i) in FIELDS"
            :key="f.id"
            ref="fields"
            class="stars"
            :style="{
                backgroundImage: images.stars[i] ? `url(${images.stars[i]})` : 'none',
                backgroundSize: `${f.tile.width * PX}px ${f.tile.height * PX}px`,
                height: `calc(100vh + ${f.tile.height * PX}px)`,
            }"
        />
        <img v-if="images.sun" ref="sun" class="sun" :src="images.sun" :width="images.size.sun" alt="" />
        <img v-if="images.moon" ref="moon" class="moon" :src="images.moon" :width="images.size.moon" alt="" />
    </div>
</template>

<style scoped lang="scss">
.sky {
    position: absolute;
    inset: 0;
    z-index: -1;
    overflow: hidden;
    background: $night-sky;
}

// Fixed to the window and shifted by their own depth, so they never run out
.stars {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    background-repeat: repeat;
    image-rendering: pixelated;
    opacity: 0;
    transition: opacity 0.8s;
    will-change: transform;

    .drawn & {
        opacity: 1;
    }
}

.sun,
.moon {
    position: absolute;
    image-rendering: pixelated;
    opacity: 0;
    transition: opacity 0.8s;
    will-change: transform;

    .drawn & {
        opacity: 1;
    }
}

// Either side of the hero's centred words: the sun high on the left, the moon lower right
.sun {
    top: 110px;
    left: max(4vw, calc(50% - 600px + 2rem));
}

.moon {
    top: 280px;
    right: max(5vw, calc(50% - 600px + 3rem));
}

@include tablet {
    .sun {
        top: 72px;
        left: 1.5rem;
    }

    .moon {
        top: 520px;
        right: 2rem;
    }
}

// A phone's hero is all words and tiles: no room for the moon
@include mobile {
    .moon {
        display: none;
    }
}
</style>
