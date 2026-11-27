// Who is using the site, as ArcLabs knows them. Aligned with terra's Identity
// (apps/cli/src/terra/registry/account.ts) and ArcLabs' device clients
// (../arclabs/apps/backend/platform/auth/device.ts).

export interface User {
    id: string
    email: string
    name: string | null
    /** The scope they publish under, without "@": made up for them at sign-up until they choose it */
    username: string | null
    /** Whether they've chosen it: once, here, before any device signs in (pages/register.vue); never changed after */
    chosen: boolean
}

/** What a person types to sign in or make an account. */
export interface Credentials {
    email: string
    password: string
}

/** What asks to sign in by a code: the game, or terra. */
export type DeviceClient = "terrafirma" | "terra-cli"
