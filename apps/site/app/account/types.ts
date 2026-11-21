// Who is using the site, as ArcLabs knows them. Aligned with terra's Identity
// (apps/cli/src/terra/registry/account.ts) and ArcLabs' device clients
// (../arclabs/apps/backend/platform/auth/device.ts).

export interface User {
    id: string
    email: string
    name: string | null
    /** The scope they publish under, without "@": null until they choose one */
    username: string | null
}

/** What a person types to sign in or make an account. */
export interface Credentials {
    email: string
    password: string
}

/** What asks to sign in by a code: the game, or terra. */
export type DeviceClient = "terrafirma" | "terra-cli"
