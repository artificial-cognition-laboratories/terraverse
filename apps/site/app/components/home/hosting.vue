<script setup lang="ts">
import { hosting, links } from "~/config/home"

const self = useCopy()
</script>

<template>
    <home-band tone="lifted">
        <home-heading :kicker="hosting.kicker" :title="hosting.title" :lede="hosting.lede" />

        <ol class="steps">
            <li v-for="(step, i) in hosting.steps" :key="step.title">
                <span class="n">{{ i + 1 }}</span>
                <div>
                    <strong>{{ step.title }}</strong>
                    <span>{{ step.text }}</span>
                </div>
            </li>
        </ol>

        <div class="plans">
            <article v-for="plan in hosting.plans" :key="plan.name" class="plan" :class="{ featured: plan.featured }">
                <header>
                    <h3>{{ plan.name }}</h3>
                    <p>{{ plan.for }}</p>
                </header>
                <div class="price">
                    <template v-if="plan.price">{{ plan.price }}</template>
                    <span v-else class="unset">Pricing at launch</span>
                </div>
                <ul>
                    <li v-for="spec in plan.specs" :key="spec">{{ spec }}</li>
                </ul>
                <home-button :to="links.server" :variant="plan.featured ? 'primary' : 'ghost'">Start a server »</home-button>
            </article>
        </div>

        <div class="self">
            <div>
                <h3>{{ hosting.self.title }}</h3>
                <p>{{ hosting.self.text }}</p>
            </div>
            <div class="self-actions">
                <button type="button" class="command" @click="self.copy(hosting.self.command)">
                    <span class="prompt">$</span> {{ hosting.self.command }}
                    <span class="state">{{ self.label.value }}</span>
                </button>
                <NuxtLink :to="links.host" class="more">How »</NuxtLink>
            </div>
        </div>
    </home-band>
</template>

<style scoped lang="scss">
.steps {
    @include on-sky;
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 1rem;
    margin: 0 0 2.5rem;
    padding: 0;
    list-style: none;

    li {
        display: flex;
        gap: 1rem;
        align-items: flex-start;
        font-family: $font-sans;
    }

    div {
        display: grid;
        gap: 0.25rem;
    }

    strong {
        color: $ink;
        font-size: 0.95rem;
    }

    span {
        color: $ink-dim;
        font-size: 0.85rem;
    }
}

.n {
    flex: none;
    display: grid;
    place-items: center;
    width: 2rem;
    height: 2rem;
    font-family: $font-display;
    font-size: 0.8rem;
    color: $primary-ink !important;
    border: 1px solid $primary-lo;
}

.plans {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 1rem;
    align-items: stretch;
}

.plan {
    @include panel($night-0);
    display: flex;
    flex-direction: column;
    gap: 1.25rem;
    padding: 1.75rem;

    // The one most people want: lifted, with the ocean along its top
    &.featured {
        position: relative;
        background: $night-2;
        border-color: $primary-lo;

        &::before {
            content: "";
            position: absolute;
            top: -1px;
            left: -1px;
            right: -1px;
            height: 3px;
            background: $primary-ink;
        }
    }

    .button {
        margin-top: auto;
        justify-content: center;
    }
}

h3 {
    margin: 0 0 0.4rem;
    font-family: $font-display;
    font-weight: 400;
    font-size: 0.95rem;
    text-transform: uppercase;
    color: $ink;
}

header p,
.self p {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.88rem;
    line-height: 1.5;
    color: $ink-dim;
}

.price {
    padding: 1rem 0;
    font-family: $font-sans;
    font-size: 1.6rem;
    font-weight: 700;
    color: $ink;
    border-top: 1px solid $night-3;
    border-bottom: 1px solid $night-3;

    .unset {
        font-family: $font-display;
        font-weight: 400;
        font-size: 0.7rem;
        text-transform: uppercase;
        color: $ink-faint;
    }
}

ul {
    margin: 0;
    padding: 0;
    list-style: none;
    font-size: 0.8rem;
    color: $ink-dim;

    li {
        padding: 0.35rem 0;

        // Green ticks: what you get, included
        &::before {
            content: "✓";
            margin-right: 0.6rem;
            color: $forest-ink;
        }
    }
}

// Below the plans, on the page itself: the free way, set apart but not boxed
.self {
    @include on-sky;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1rem;
    margin-top: 3.5rem;
    text-align: center;

    > div {
        max-width: 34rem;
    }

    h3 {
        font-size: 0.75rem;
        margin-bottom: 0.6rem;
    }
}

.self-actions {
    display: flex;
    gap: 1.5rem;
    align-items: center;
}

.command {
    flex: none;
    padding: 0.7rem 1rem;
    font-family: $font-mono;
    font-size: 0.85rem;
    color: $ink;
    background: $night-1;
    border: 1px solid $night-3;
    cursor: pointer;

    .prompt {
        color: $primary-ink;
    }

    .state {
        margin-left: 0.8rem;
        color: $ink-faint;
    }

    &:hover {
        border-color: $night-4;

        .state {
            color: $primary-ink;
        }
    }
}

.more {
    flex: none;
    font-family: $font-display;
    font-size: 0.65rem;
    text-transform: uppercase;
    color: $primary-ink;
    text-decoration: none;

    &:hover {
        color: $ink;
    }
}

@include tablet {
    .steps,
    .plans {
        grid-template-columns: minmax(0, 1fr);
    }

}
</style>
