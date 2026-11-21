<script setup lang="ts">
import { agents, links } from "~/config/home"
</script>

<template>
    <home-band tone="lifted">
        <div class="split">
            <home-window class="terminal" title="agent · @scout" note="headless">
                <ol class="log">
                    <li v-for="(line, i) in agents.transcript" :key="i" :class="line.who">
                        <span class="who">{{ line.who === "agent" ? "»" : " " }}</span>
                        <span>{{ line.text }}</span>
                    </li>
                    <li class="agent"><span class="who">»</span><span class="cursor" /></li>
                </ol>
            </home-window>
            <div>
                <home-heading :kicker="agents.kicker" :title="agents.title" :lede="agents.lede" />
                <home-points :points="agents.points" />
                <div class="actions">
                    <home-button :to="links.docs" variant="primary">The agent API »</home-button>
                </div>
            </div>
        </div>
    </home-band>
</template>

<style scoped lang="scss">
.split {
    display: grid;
    grid-template-columns: minmax(0, 6fr) minmax(0, 5fr);
    gap: 4rem;
    align-items: center;
}

.log {
    margin: 0;
    padding: 1.25rem 1.25rem 1.5rem;
    list-style: none;
    font-family: $font-mono;
    font-size: 0.8rem;
    line-height: 1.6;
    background: $stone-0;
    min-height: 20rem;

    li {
        display: flex;
        gap: 0.75rem;
        padding: 0.2rem 0;
    }

    .agent {
        color: $ink;
    }

    .world {
        color: rgba($forest-ink, 0.75);
        padding-bottom: 0.6rem;
    }

    .who {
        flex: none;
        width: 1ch;
        color: $primary-ink;
    }
}

.cursor {
    width: 0.6em;
    height: 1.1em;
    background: $primary-ink;
    animation: blink 1.1s steps(1) infinite;
}

@keyframes blink {
    50% {
        opacity: 0;
    }
}

.actions {
    margin-top: 2.25rem;
}

@include tablet {
    .split {
        grid-template-columns: minmax(0, 1fr);
        gap: 3rem;
    }

    .terminal {
        order: 2;
    }
}

@media (prefers-reduced-motion: reduce) {
    .cursor {
        animation: none;
    }
}
</style>
