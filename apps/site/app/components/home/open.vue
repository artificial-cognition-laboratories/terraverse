<script setup lang="ts">
import { GITHUB } from "~/config/site"
import { links, open } from "~/config/home"
</script>

<template>
    <home-band>
        <div id="download" class="split">
            <div>
                <home-heading :kicker="open.kicker" :title="open.title" :lede="open.lede" />
                <div class="actions">
                    <home-button :to="GITHUB"><Icon name="simple-icons:github" /> GitHub</home-button>
                    <home-button :to="links.install">Install guide</home-button>
                </div>
            </div>
            <home-download />
        </div>
    </home-band>
</template>

<style scoped lang="scss">
.split {
    display: grid;
    grid-template-columns: minmax(0, 4fr) minmax(0, 7fr);
    gap: 4rem;
    align-items: center;
    // The topnav is fixed: an anchor lands below it
    scroll-margin-top: 6rem;
}

.actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
}

@include desktop-down {
    .split {
        grid-template-columns: minmax(0, 1fr);
        gap: 2.5rem;
    }
}
</style>
