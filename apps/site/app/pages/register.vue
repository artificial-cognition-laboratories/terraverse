<script setup lang="ts">
import { Refused, returnTo, useAccount, username } from "~/account"

definePageMeta({ layout: "landing" })
useHead({ title: "Create an account", meta: [{ name: "robots", content: "noindex" }] })

/**
 * Two steps: an account, then the name you publish under. Signing in with an
 * account whose name hasn't been chosen lands here at the second step, the
 * name made up for it at sign-up offered to keep.
 */
const route = useRoute()
const account = useAccount()
const next = computed(() => returnTo(route.query.next))

const name = ref("")
const busy = ref(false)
const error = ref<string | null>(null)
const problem = computed(() => (name.value ? username.problem(name.value) : null))

watch(
    account.user,
    user => {
        if (user?.chosen) void navigateTo(next.value, { replace: true })
        else if (user && !name.value) name.value = user.username ?? ""
    },
    { immediate: true },
)

async function run(work: () => Promise<unknown>) {
    busy.value = true
    error.value = null
    try {
        await work()
    } catch (e) {
        if (!(e instanceof Refused)) throw e
        error.value = e.message
    } finally {
        busy.value = false
    }
}
</script>

<template>
    <auth-card v-if="!account.user.value" title="Create your account" lede="One account for the game, the registry and your servers.">
        <auth-sign action="Create account" :google="account.google" :submit="account.register" new-password />
        <template #fine>
            Have an account? <NuxtLink :to="{ path: '/signin', query: { next: next } }">Sign in</NuxtLink>
        </template>
    </auth-card>

    <auth-card v-else title="Choose your name">
        <template #lede>
            Signed in as <strong>{{ account.user.value.email }}</strong>. Your name is your page on the registry and the
            scope every mod you publish lives under. It can't be changed later.
        </template>
        <form class="claim" @submit.prevent="run(() => account.claim(name))">
            <label class="field" :class="{ bad: problem }">
                <span class="at">@</span>
                <input v-model.trim="name" type="text" placeholder="yourname" autocomplete="username" spellcheck="false" aria-label="Username" autofocus />
            </label>
            <p v-if="problem || error" class="error" role="alert">{{ problem ?? error }}</p>
            <p v-else class="preview">
                <span class="prompt">$</span> terra install <span class="scope">@{{ name || "yourname" }}</span>/your-mod
            </p>
            <home-button type="submit" variant="primary" size="lg" class="go" :disabled="busy || !name || !!problem">
                {{ busy ? "Claiming…" : `Claim @${name || "…"}` }}
            </home-button>
        </form>
    </auth-card>
</template>

<style scoped lang="scss">
.claim {
    display: grid;
    gap: 0.85rem;
}

.field {
    @include well;
    display: flex;
    align-items: center;
    height: 2.8rem;
    padding: 0 0.9rem;
    font-family: $font-mono;
    font-size: 0.95rem;
    min-width: 0;

    &:focus-within {
        border-color: rgba($primary-ink, 0.55);
    }

    &.bad {
        border-color: rgba($status-bad, 0.6);
    }

    .at {
        color: $primary-ink;
    }

    input {
        flex: 1;
        min-width: 0;
        padding: 0 0 0 0.15rem;
        font: inherit;
        color: $ink;
        background: none;
        border: none;
        outline: none;

        &::placeholder {
            color: $ink-faint;
        }
    }
}

.error,
.preview {
    margin: 0;
    font-size: 0.8rem;
}

.error {
    font-family: $font-sans;
    color: $status-bad;
}

.preview {
    font-family: $font-mono;
    color: $ink-faint;

    .prompt,
    .scope {
        color: $primary-ink;
    }
}

.go {
    justify-content: center;
    width: 100%;
}
</style>
