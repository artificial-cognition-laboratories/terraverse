<script setup lang="ts">
import { useAccount } from "~/account"
import { format } from "~/lib/format"

/**
 * The account, top-right of the topnav: "sign in" when nobody is, and when
 * someone is, their mark, opening onto their page, their stars and signing out.
 * Nothing until this browser has been read, so the prerendered page never
 * flashes the wrong one.
 */
const route = useRoute()
const account = useAccount()
const open = ref(false)
const root = ref<HTMLElement | null>(null)

const user = account.user
const handle = computed(() => (user.value?.username ? `@${user.value.username}` : (user.value?.name ?? user.value?.email ?? "")))

watch(() => route.fullPath, () => (open.value = false))

function outside(event: MouseEvent) {
    if (open.value && root.value && event.target instanceof Node && !root.value.contains(event.target)) open.value = false
}
onMounted(() => document.addEventListener("click", outside))
onBeforeUnmount(() => document.removeEventListener("click", outside))

function signOut() {
    void account.signOut()
    open.value = false
}
</script>

<template>
    <div v-if="account.ready.value" ref="root" class="profile">
        <NuxtLink v-if="!user" :to="{ path: '/signin', query: { next: route.fullPath } }" class="signin">sign in</NuxtLink>
        <template v-else>
            <button type="button" class="me" :aria-expanded="open" aria-label="Your account" @click="open = !open">
                {{ format.monogram(user.username ?? user.name ?? user.email) }}
            </button>
            <div v-if="open" class="menu" role="menu">
                <div class="who">
                    <strong>{{ handle }}</strong>
                    <span>{{ user.email }}</span>
                </div>
                <NuxtLink v-if="user.username" :to="`/registry/@${user.username}`" role="menuitem">Your page</NuxtLink>
                <NuxtLink v-else to="/register" role="menuitem">Choose your name</NuxtLink>
                <NuxtLink to="/registry/starred" role="menuitem">Starred</NuxtLink>
                <button type="button" role="menuitem" @click="signOut">Sign out</button>
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

.signin {
    font-size: 0.85rem;
    color: $ink-dim;
    text-decoration: none;

    &:hover {
        color: $ink;
    }
}

.me {
    @include well;
    display: grid;
    place-items: center;
    width: 1.9rem;
    height: 1.9rem;
    font-family: $font-display;
    font-size: 0.6rem;
    color: $primary-ink;
    cursor: pointer;

    &:hover,
    &[aria-expanded="true"] {
        border-color: rgba($primary-ink, 0.55);
    }
}

.menu {
    position: absolute;
    top: calc(100% + 0.6rem);
    right: 0;
    display: grid;
    min-width: 13rem;
    padding: 0.35rem;
    background: rgba($night-0, 0.96);
    backdrop-filter: blur(10px);
    border: 1px solid $glass-edge;

    a,
    button {
        padding: 0.55rem 0.7rem;
        font-family: $font-sans;
        font-size: 0.85rem;
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

    button {
        border-top: 1px solid $glass-edge;
    }
}

.who {
    display: grid;
    gap: 0.15rem;
    padding: 0.55rem 0.7rem 0.7rem;
    margin-bottom: 0.25rem;
    border-bottom: 1px solid $glass-edge;

    strong {
        font-family: $font-mono;
        font-weight: 400;
        font-size: 0.85rem;
        color: $ink;
    }

    span {
        font-family: $font-sans;
        font-size: 0.75rem;
        color: $ink-faint;
    }
}
</style>
