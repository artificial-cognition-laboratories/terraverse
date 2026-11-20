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
        <h1 :aria-label="TITLE">
            <!-- each letter drops into place, like a block being set -->
            <span v-for="(letter, i) in TITLE" :key="i" class="letter" :class="{ gap: letter === ' ' }" :style="{ '--i': i }"
                aria-hidden="true">{{ letter }}</span>
            <!-- the same word again, only as a light passing over it -->
            <span class="glint" aria-hidden="true">{{ TITLE }}</span>
        </h1>
        <p class="tagline">a whole planet. every block of it.</p>

        <nav>
            <Button variant="primary" size="lg" @click="$emit('go', 'worlds')">Play</Button>
            <Button size="lg" @click="$emit('go', 'settings')">Settings</Button>
            <div class="pair">
                <Button size="lg" @click="$emit('go', 'community')">Community</Button>
                <Button size="lg" @click="bridge.send('quit', {})">Quit</Button>
            </div>
        </nav>
    </div>
</template>

<style scoped>
.home { display: flex; flex-direction: column; align-items: center; }

h1 {
    position: relative;
    display: flex;
    margin: 0;
    font: 400 clamp(56px, 7.2vw, 124px) / 1 var(--font-display);
    text-transform: uppercase;
    letter-spacing: 0.04em;
    white-space: nowrap;
    color: #eef3f6;
}

/* stepped extrusion: a block's depth, in the accent's darks, then its shadow */
.letter {
    display: inline-block;
    text-shadow:
        2px 2px 0 #1c6f96,
        4px 4px 0 #13536f,
        6px 6px 0 #0a3a52,
        8px 8px 0 #062636,
        14px 14px 0 rgb(0 0 0 / 0.4);
    animation: set 420ms steps(5) both;
    animation-delay: calc(var(--i) * 45ms);
}

.gap { width: 0.42em; } /* the space between the words: tighter than the font's */

.glint {
    word-spacing: -0.45em; /* matches .gap */
    position: absolute;
    inset: 0;
    letter-spacing: inherit;
    color: transparent;
    background: linear-gradient(105deg, transparent 40%, rgb(255 255 255 / 0.85) 50%, transparent 60%) no-repeat;
    background-size: 250% 100%;
    background-clip: text;
    -webkit-background-clip: text;
    pointer-events: none;
    animation: glint 7s steps(28) 1.2s infinite;
}

.tagline {
    margin: 26px 0 52px;
    font: 10px var(--font);
    letter-spacing: 0.32em;
    text-transform: uppercase;
    color: var(--ink-dim);
}

nav { display: grid; gap: 12px; width: min(420px, 84vw); }
nav > :deep(.btn) { width: 100%; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
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

@media (max-width: 850px) {
    h1 { font-size: clamp(40px, 9vw, 76px); }
    .tagline { letter-spacing: 0.18em; }
}
</style>
