<script setup lang="ts">
import { useAccount } from "~/account"
import { format } from "~/lib/format"

/**
 * The account, top-right of the topnav: always a circle. Signed out, a figure
 * leading to sign-in; signed in, their mark, opening onto their page, their
 * stars and signing out. Drawn from who was here last until the session is
 * read, so it's right on the first frame and never shifts the topnav.
 */
const route = useRoute()
const account = useAccount()
const open = ref(false)
const root = ref<HTMLElement | null>(null)

const user = account.seen
const handle = computed(() => (user.value?.username ? `@${user.value.username}` : (user.value?.name ?? user.value?.email ?? "")))
const mark = computed(() => (user.value ? format.monogram(user.value.username ?? user.value.name ?? user.value.email) : ""))

watch(() => route.fullPath, () => (open.value = false))

function outside(event: MouseEvent) {
    if (open.value && root.value && event.target instanceof Node && !root.value.contains(event.target)) open.value = false
}
function escape(event: KeyboardEvent) {
    if (event.key === "Escape") open.value = false
}
onMounted(() => {
    document.addEventListener("click", outside)
    document.addEventListener("keydown", escape)
})
onBeforeUnmount(() => {
    document.removeEventListener("click", outside)
    document.removeEventListener("keydown", escape)
})

// The way back is added on the click, never in the link: a prerendered page's
// link is crawled, and /signin's own would lead to ?next=/signin?next=… forever
function signIn() {
    const back = ["/signin", "/register"].includes(route.path) ? undefined : route.fullPath
    void navigateTo({ path: "/signin", query: back ? { next: back } : {} })
}

function signOut() {
    void account.signOut()
    open.value = false
}
</script>

<template>
    <div ref="root" class="profile">
        <NuxtLink v-if="!user" to="/signin" class="me" @click.prevent="signIn" aria-label="Sign in" title="Sign in">
            <svg viewBox="0 0 8 8" shape-rendering="crispEdges" aria-hidden="true">
                <path fill="currentColor" d="M3 1h2v1h1v2H5v1H3V4H2V2h1zM1 6h6v2H1z" />
            </svg>
        </NuxtLink>
        <template v-else>
            <button type="button" class="me signed" :aria-expanded="open" aria-label="Your account" @click="open = !open">
                {{ mark }}
            </button>
            <div v-if="open" class="menu" role="menu">
                <div class="who">
                    <strong>{{ handle }}</strong>
                    <span>{{ user.email }}</span>
                </div>
                <div class="group">
                    <NuxtLink v-if="user.username" :to="`/registry/@${user.username}`" role="menuitem">Your page</NuxtLink>
                    <NuxtLink v-else to="/register" role="menuitem">Choose your name</NuxtLink>
                    <NuxtLink to="/registry/starred" role="menuitem">Starred</NuxtLink>
                </div>
                <div class="group">
                    <button type="button" role="menuitem" @click="signOut">Sign out</button>
                </div>
            </div>
        </template>
    </div>
</template>

<style scoped lang="scss">
.profile {
    position: relative;
    display: flex;
    align-items: center;
}

.me {
    @include well;
    display: grid;
    place-items: center;
    width: 1.9rem;
    height: 1.9rem;
    padding: 0;
    font-family: $font-display;
    font-size: 0.6rem;
    color: $ink-dim;
    text-decoration: none;
    cursor: pointer;

    svg {
        width: 0.85rem;
        height: 0.85rem;
    }

    &.signed {
        color: $primary-ink;
    }

    &:hover,
    &[aria-expanded="true"] {
        color: $ink;
        border-color: rgba($primary-ink, 0.55);
    }
}

.menu {
    position: absolute;
    // Hung from the topnav's bottom line, its top edge on that line, so the two never cross
    top: calc(100% + (#{$topnav-height} - 1.9rem) / 2);
    right: 0;
    z-index: 20;
    width: 15rem;
    background: rgba($night-0, 0.97);
    backdrop-filter: blur(10px);
    border: 1px solid $glass-edge;
    border-radius: $radius;

    a,
    button {
        display: block;
        width: 100%;
        padding: 0.5rem 1rem;
        font-family: $font-sans;
        font-size: 0.82rem;
        text-align: left;
        color: $ink-dim;
        text-decoration: none;
        background: none;
        border: none;
        cursor: pointer;

        &:hover {
            color: $ink;
            background: rgba($primary-ink, 0.07);
        }
    }
}

.who {
    display: grid;
    gap: 0.2rem;
    min-width: 0;
    padding: 0.9rem 1rem;
    border-bottom: 1px solid $glass-edge;

    strong,
    span {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    strong {
        font-family: $font-mono;
        font-weight: 400;
        font-size: 0.82rem;
        color: $ink;
    }

    span {
        font-family: $font-sans;
        font-size: 0.72rem;
        color: $ink-faint;
    }
}

.group {
    padding: 0.35rem 0;

    & + & {
        border-top: 1px solid $glass-edge;
    }
}
</style>
