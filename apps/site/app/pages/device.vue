<script setup lang="ts">
import { DEVICE_CODE, Refused, useAccount, type DeviceClient } from "~/account"

definePageMeta({ layout: "landing" })
useHead({ title: "Authorize a device", meta: [{ name: "robots", content: "noindex" }] })

/**
 * Where a code from the game or terra is approved: /device?code=ABC-234&client=terra-cli,
 * the address ArcLabs' device flow hands out. Each state below is one card.
 */
const route = useRoute()
const account = useAccount()
const copy = useCopy()

// What each client is, and what approving lets it do: said before anyone clicks Authorize
const CLIENTS: Record<DeviceClient, { name: string; does: string }> = {
    terrafirma: { name: "Terraverse", does: "play online as you: your friends, hosted worlds and servers" },
    "terra-cli": { name: "terra", does: "install, publish and deploy as you" },
}
const client = computed((): DeviceClient => (route.query.client === "terrafirma" ? "terrafirma" : "terra-cli"))
const asker = computed(() => CLIENTS[client.value])
const code = computed(() => (typeof route.query.code === "string" ? route.query.code.trim().toUpperCase() : ""))

const typed = ref("")
const busy = ref(false)
const error = ref<string | null>(null)
const outcome = ref<"approved" | "cancelled" | null>(null)

type State = "enter" | "signin" | "name" | "confirm" | "approved" | "cancelled"
const state = computed((): State => {
    if (outcome.value) return outcome.value
    if (!DEVICE_CODE.test(code.value)) return "enter"
    const user = account.user.value
    if (!user) return "signin"
    if (client.value === "terra-cli" && !user.username) return "name"
    return "confirm"
})

function enter() {
    const value = typed.value.trim().toUpperCase().replace(/^([A-Z0-9]{3})([A-Z0-9]{3})$/, "$1-$2")
    if (!DEVICE_CODE.test(value)) {
        error.value = "That isn't a device code: it looks like ABC-234."
        return
    }
    error.value = null
    void navigateTo({ query: { ...route.query, code: value } })
}

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

async function approve() {
    await account.device.approve(code.value, client.value)
    outcome.value = "approved"
}
</script>

<template>
    <auth-card v-if="state === 'enter'" title="Connect a device" lede="Enter the code shown in the game or in your terminal.">
        <form class="enter" @submit.prevent="enter">
            <input v-model="typed" class="code-input" type="text" placeholder="ABC-234" maxlength="7" autocomplete="off" spellcheck="false" aria-label="Device code" autofocus />
            <p v-if="error" class="error" role="alert">{{ error }}</p>
            <home-button type="submit" variant="primary" size="lg" class="go">Continue</home-button>
        </form>
    </auth-card>

    <auth-card v-else-if="state === 'signin'" :title="`Connect ${asker.name}`">
        <template #lede>Sign in to let {{ asker.name }} {{ asker.does }}.</template>
        <button type="button" class="code" :aria-label="`Copy the code ${code}`" @click="copy.copy(code)">
            <span class="label">Device code · {{ copy.label.value }}</span>
            <span class="value">{{ code }}</span>
        </button>
        <auth-sign action="Sign in" :google="account.google" :submit="account.signIn" />
        <template #fine>
            No account? <NuxtLink :to="{ path: '/register', query: { next: route.fullPath } }">Create one</NuxtLink>
        </template>
    </auth-card>

    <auth-card v-else-if="state === 'name'" title="Choose your name first">
        <template #lede>terra publishes under your name, and you haven't chosen one yet. Choose it, then come back to this code.</template>
        <home-button variant="primary" size="lg" class="go" :to="`/register?next=${encodeURIComponent(route.fullPath)}`">Choose a name</home-button>
    </auth-card>

    <auth-card v-else-if="state === 'confirm'" title="Is this the code?">
        <template #lede>
            Check it matches the one {{ asker.name }} shows. Authorizing lets it {{ asker.does }}, as
            <strong>{{ account.user.value?.username ? `@${account.user.value.username}` : account.user.value?.email }}</strong>.
        </template>
        <div class="code still">
            <span class="label">Device code</span>
            <span class="value">{{ code }}</span>
        </div>
        <div class="choices">
            <p v-if="error" class="error" role="alert">{{ error }}</p>
            <home-button variant="primary" size="lg" class="go" :disabled="busy" @click="run(approve)">
                {{ busy ? "Authorizing…" : `Authorize ${asker.name}` }}
            </home-button>
            <home-button size="lg" class="go" :disabled="busy" @click="outcome = 'cancelled'">It doesn't match</home-button>
        </div>
        <template #fine>
            Not you? <a href="#" @click.prevent="void account.signOut()">Sign in as someone else</a>
        </template>
    </auth-card>

    <auth-card v-else-if="state === 'approved'" :title="`${asker.name} is signed in`">
        <template #lede>Go back to {{ client === "terra-cli" ? "your terminal" : "the game" }}: it's carrying on. You can close this tab.</template>
        <p class="done"><span class="tick">✓</span> {{ code }} authorized</p>
    </auth-card>

    <auth-card v-else title="Nothing was signed in" lede="The code was left alone. If something asked you for one you didn't expect, ignore it: it can't sign in without you.">
        <home-button size="lg" class="go" to="/">Back to Terraverse</home-button>
    </auth-card>
</template>

<style scoped lang="scss">
.enter,
.choices {
    display: grid;
    gap: 0.75rem;
}

.go {
    justify-content: center;
    width: 100%;
}

.code-input {
    @include well;
    width: 100%;
    min-width: 0;
    height: 3.6rem;
    padding: 0 1rem;
    font-family: $font-mono;
    font-size: 1.6rem;
    letter-spacing: 0.25em;
    text-align: center;
    text-transform: uppercase;
    color: $ink;
    outline: none;

    &:focus {
        border-color: rgba($primary-ink, 0.55);
    }

    &::placeholder {
        color: $ink-faint;
    }
}

// The code itself: big, mono, and the thing to compare
.code {
    @include well;
    display: grid;
    justify-items: center;
    gap: 0.5rem;
    padding: 1.1rem 1rem 1.2rem;
    cursor: pointer;

    &.still {
        cursor: default;
    }

    &:not(.still):hover {
        border-color: rgba($primary-ink, 0.55);
    }

    .label {
        font-family: $font-display;
        font-size: 0.58rem;
        text-transform: uppercase;
        color: $ink-faint;
    }

    .value {
        font-family: $font-mono;
        font-size: 1.9rem;
        letter-spacing: 0.22em;
        color: $ink;
    }
}

.error {
    margin: 0;
    font-family: $font-sans;
    font-size: 0.8rem;
    color: $status-bad;
}

.done {
    margin: 0;
    font-family: $font-mono;
    font-size: 0.85rem;
    color: $ink-dim;

    .tick {
        color: $forest-ink;
    }
}
</style>
