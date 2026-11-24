// mod.config.ts and server.config.ts: read without running any mod.
export {}

declare global {
    /** What a server can choose about a mod, set once per server and held for every world on it. */
    type SettingDeclaration =
        | { type: "number"; default: number; range?: readonly [number, number]; step?: number; unit?: string; description?: string }
        | { type: "boolean"; default: boolean; description?: string }
        | { type: "choice"; default: string; options: Record<string, string>; description?: string }
        | { type: "string"; default: string; pattern?: string; description?: string }

    interface ModConfig {
        /** How players see the mod. Its id is in package.json. */
        title?: string
        /** What a server may choose: `mod.settings` in the mod, checked before it runs. */
        settings?: Record<string, SettingDeclaration>
        /** What it's for on the registry: one to three, from a fixed list, so Browse's filters mean one thing. Needed to publish. */
        categories?: readonly ModCategory[]
    }
    /** The registry's categories (ArcLabs: platform/terrafirma/categories.ts holds the same list). */
    type ModCategory = "games" | "survival" | "building" | "machines" | "matter" | "world" | "textures" | "library"
    function defineMod<C extends ModConfig>(config: C): C

    interface ServerConfig {
        /** How players see the server. Its id is in package.json. */
        title?: string
        /**
         * The star system, as a world code: "TF9.dGVycmE". Fixed once the universe is made:
         * changing it would be a different universe.
         */
        system: string
        /**
         * The worlds players can be on, by name: each a body of the system ("@home" for
         * the one the system is generated around). Not used yet: every body is there.
         */
        worlds?: Record<string, string>
        /** Each mod's settings, by package name: typed from what the mods declare, checked before they run. */
        settings?: ServerSettings
        access?: {
            /** Who may join. Not enforced yet. */
            join?: "anyone" | "friends" | "invited"
            /** Accounts by role. Not used yet. */
            roles?: Record<string, readonly AccountName[]>
        }
    }
    /** The settings of the mods this server runs (prepare writes them into settings.d.ts). */
    interface ServerSettings {}

    function defineServer<C extends ServerConfig>(config: C): C
}
