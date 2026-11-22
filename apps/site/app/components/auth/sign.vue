<script setup lang="ts">
import { Refused, type Credentials } from "~/account"

/**
 * The way in, on every account screen: Google, or an email and password. It
 * owns the form; what signing in means (in, or a new account) is `submit`'s,
 * and a refusal it throws is said under the button.
 */
const props = defineProps<{ action: string; google: () => Promise<unknown>; submit: (c: Credentials) => Promise<unknown>; newPassword?: boolean }>()

const email = ref("")
const password = ref("")
const busy = ref<"google" | "email" | null>(null)
const error = ref<string | null>(null)

async function run(which: "google" | "email", work: () => Promise<unknown>) {
    busy.value = which
    error.value = null
    try {
        await work()
    } catch (e) {
        if (!(e instanceof Refused)) throw e
        error.value = e.message
    } finally {
        busy.value = null
    }
}
</script>

<template>
    <div class="sign">
        <button type="button" class="google" :disabled="!!busy" @click="run('google', props.google)">
            <Icon name="simple-icons:google" class="mark" />
            {{ busy === "google" ? "Opening Google…" : "Continue with Google" }}
        </button>

        <p class="or"><span>or</span></p>

        <form class="form" @submit.prevent="run('email', () => props.submit({ email: email, password: password }))">
            <auth-field v-model.trim="email" label="Email" type="email" autocomplete="email" placeholder="you@example.com" />
            <auth-field
                v-model="password"
                label="Password"
                type="password"
                :autocomplete="newPassword ? 'new-password' : 'current-password'"
                :placeholder="newPassword ? 'At least 8 characters' : ''"
            />
            <p v-if="error" class="error" role="alert">{{ error }}</p>
            <home-button type="submit" variant="primary" size="lg" class="go" :disabled="!!busy || !email || !password">
                {{ busy === "email" ? "One moment…" : action }}
            </home-button>
        </form>
    </div>
</template>

<style scoped lang="scss">
.sign {
    display: grid;
    gap: 1.5rem;
}

.google {
    @include panel;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.7rem;
    height: 3rem;
    font-family: $font-sans;
    font-size: 0.92rem;
    font-weight: 500;
    color: $ink;
    cursor: pointer;

    &:hover:not(:disabled) {
        border-color: rgba($primary-ink, 0.55);
    }

    &:disabled {
        cursor: default;
        opacity: 0.6;
    }

    .mark {
        width: 1rem;
        height: 1rem;
    }
}

// A hairline either side of the word
.or {
    display: flex;
    align-items: center;
    gap: 0.9rem;
    margin: 0;
    font-family: $font-display;
    font-size: 0.55rem;
    text-transform: uppercase;
    color: $ink-faint;

    &::before,
    &::after {
        content: "";
        flex: 1;
        border-top: 1px solid $glass-edge;
    }
}

.form {
    display: grid;
    gap: 1.1rem;
}

.error {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.8rem;
    color: $status-bad;
}

.go {
    justify-content: center;
    width: 100%;
    margin-top: 0.35rem;
}
</style>
