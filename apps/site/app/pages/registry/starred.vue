<script setup lang="ts">
import { useAccount } from "~/account"
import { useRegistry } from "~/registry"

definePageMeta({ layout: "landing" })
useHead({ title: "Starred", meta: [{ name: "robots", content: "noindex" }] })

// Your stars live with your account, so this page is drawn in the browser, never prerendered with anyone's
const account = useAccount()
const reg = useRegistry()
const names = computed(() => account.stars.list())
const { data: listings } = await useLive(() => `registry:starred:${names.value.join(",")}`, () => reg.listings(names.value))
</script>

<template>
    <registry-shell>
        <header class="head">
            <h1>Starred</h1>
            <p v-if="account.user.value">What you've starred, newest last. Star a mod from its page.</p>
        </header>

        <div v-if="!account.ready.value" />
        <div v-else-if="!account.user.value" class="empty">
            <h2>Sign in to see your stars.</h2>
            <home-button variant="primary" :to="{ path: '/signin', query: { next: '/registry/starred' } }">Sign in</home-button>
        </div>
        <registry-cards v-else-if="listings?.length" :listings="listings" />
        <div v-else-if="listings" class="empty">
            <h2>Nothing starred yet.</h2>
            <home-button to="/registry/browse">Browse the registry »</home-button>
        </div>
    </registry-shell>
</template>

<style scoped lang="scss">
.head {
    @include on-sky;
    margin-bottom: 2rem;

    h1 {
        margin: 0;
        font-family: $font-sans;
        font-weight: 600;
        font-size: 1.8rem;
        color: $ink;
    }

    p {
        margin: 0.4rem 0 0;
        font-family: $font-sans;
        font-size: 0.9rem;
        color: $ink-dim;
    }
}

.empty {
    @include panel;
    display: grid;
    justify-items: center;
    gap: 1rem;
    padding: 3rem 1.5rem;
    text-align: center;

    h2 {
        margin: 0;
        font-family: $font-sans;
        font-weight: 600;
        font-size: 1.1rem;
        color: $ink;
    }
}
</style>
