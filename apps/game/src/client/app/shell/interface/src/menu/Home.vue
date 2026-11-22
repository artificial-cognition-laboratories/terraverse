<script setup lang="ts">
import { Button } from "../kit"
import { useBridge } from "../bridge/use"
import type { Screen } from "./screens"

defineEmits<{ go: [screen: Screen] }>()
const bridge = useBridge()
const TITLE = "Terraverse"
</script>

<template>
    <div class="home">
        <p class="kicker"><span aria-hidden="true">»</span> A universe to play in</p>
        <h1 :aria-label="TITLE">
            <!-- each letter drops into place, like a block being set -->
            <span v-for="(letter, i) in TITLE" :key="i" class="letter" :style="{ '--i': i }" aria-hidden="true">{{ letter }}</span>
            <!-- the same word again, only as a light passing over it -->
            <span class="glint" aria-hidden="true">{{ TITLE }}</span>
        </h1>
        <p class="lede">A whole planet. Every block of it.</p>

        <nav>
            <Button variant="primary" size="lg" @click="$emit('go', 'worlds')">Play »</Button>
            <Button size="lg" @click="$emit('go', 'settings')">Settings</Button>
            <div class="pair">
                <Button size="lg" @click="$emit('go', 'library')">Library</Button>
                <Button size="lg" @click="bridge.send('quit', {})">Quit</Button>
            </div>
        </nav>
    </div>
</template>

<style scoped>
/* The website's hero (apps/site home/hero.vue): a blue kicker, the word in the pixel face with
   one ledge under it, a sans line beneath. The halo (the site's `on-sky`) clears the stars from
   inside the letters; it's the night's own colour, so it never reads as a shape. */
.home {
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
    text-shadow: 0 0 3px var(--night-0), 0 0 6px var(--night-0), 0 0 10px var(--night-0);
}

.kicker {
    margin: 0;
    font: 0.75rem var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--accent-hi);
}

h1 {
    position: relative;
    display: flex;
    /* Silkscreen's line box carries air above and below the caps: tight margins keep the
       kicker and the lede with the word, and the buttons stand apart */
    margin: 0 0 0.4rem;
    font: 400 clamp(4rem, 9vw, 9rem) / 0.9 var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.02em;
    white-space: nowrap;
    color: var(--ink);
}

.letter {
    display: inline-block;
    text-shadow: 0 0.06em 0 var(--edge), 0 0 6px var(--night-0), 0 0 14px var(--night-0);
    animation: set 420ms steps(5) both;
    animation-delay: calc(var(--i) * 45ms);
}

.glint {
    position: absolute;
    inset: 0;
    letter-spacing: inherit;
    color: transparent;
    text-shadow: none;
    background: linear-gradient(105deg, transparent 40%, rgb(255 255 255 / 0.7) 50%, transparent 60%) no-repeat;
    background-size: 250% 100%;
    background-clip: text;
    -webkit-background-clip: text;
    pointer-events: none;
    animation: glint 7s steps(28) 1.2s infinite;
}

.lede {
    margin: 0 0 3rem;
    font: 1.1rem/1.65 var(--font-sans);
    color: var(--ink-dim);
}

nav { display: grid; gap: 0.75rem; width: min(26rem, 84vw); text-shadow: none; }
nav > :deep(.btn) { width: 100%; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 0.75rem; }
.pair :deep(.btn) { width: 100%; }

@keyframes set { from { opacity: 0; transform: translateY(-0.4em); } }
/* across once, then a long rest */
@keyframes glint {
    0% { background-position: 120% 0; }
    18%, 100% { background-position: -20% 0; }
}

@media (prefers-reduced-motion: reduce) {
    .letter, .glint { animation: none; }
}
</style>
