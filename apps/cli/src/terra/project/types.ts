import type { ProjectKind } from "./kinds"
import type { SettingDeclarations, SettingValue } from "./settings"

// What a prepared project is, and the .terra/ contract between terra and the sidecar.

/** One mod as the sidecar loads it. */
export type ManifestMod = {
    readonly name: string           // "@you/kilns"
    readonly prefix: string         // "kilns"
    readonly version: string
    /** The mod's folder: its assets/ are here. A local mod's own folder, never bun's copy. */
    readonly root: string
    /** Its server half, bundled. null for a mod with no server/main.ts. */
    readonly server: string | null
    /** Its settings as this server chose them, every default filled in. */
    readonly settings: Readonly<Record<string, SettingValue>>
}

/** .terra/manifest.json. */
export type ManifestData = {
    readonly version: 1
    /** In load order: every mod after the mods it builds on. */
    readonly mods: readonly ManifestMod[]
}

/** A mod as a server declares it, before it's built. */
export type Mod = {
    readonly name: string
    readonly prefix: string
    readonly version: string
    readonly root: string
    readonly title: string
    /** A file: dependency, in the server's own folder: its source is the server's to edit. */
    readonly local: boolean
    /** What it lets a server choose. */
    readonly settings: SettingDeclarations
    /** The mods it builds on, by name: each loads before it. */
    readonly needs: readonly string[]
}

/** A package.json, as much of it as terra reads. */
export type PackageData = {
    readonly name: string
    readonly version: string
    readonly dependencies: Readonly<Record<string, string>>
}

/** A config file, as much of it as terra reads. */
export type ConfigData = {
    /** How players see it. Defaults to the package's name. */
    readonly title: string
    /** What it lets a server choose: a mod's declarations; none for other kinds. */
    readonly settings: SettingDeclarations
}

/** server.config.ts, checked: how a universe is set up. */
export type ServerConfigData = {
    readonly title: string
    /** The star system, as a world code. Fixed once the universe is made. */
    readonly system: string
    /** World names → bodies of the system ("@home"). Not used yet. */
    readonly worlds: Readonly<Record<string, string>>
    /** Each mod's settings, by package name. Not checked yet. */
    readonly settings: Readonly<Record<string, Readonly<Record<string, unknown>>>>
    readonly access: {
        readonly join: "anyone" | "friends" | "invited"
        readonly roles: Readonly<Record<string, readonly string[]>>
    }
}

export type PrepareStep = "install" | "config" | "types" | "mods" | "build"

export type PrepareInput = {
    /** Each step as it starts: a prepare can take seconds, and a caller may show them. */
    progress?: (step: PrepareStep) => void
}

type PreparedBase = {
    readonly root: string
    readonly name: string
    readonly title: string
    readonly installed: boolean
}

/** A server, prepared: its mods built and its manifest written. The engine takes only this. */
export type PreparedServer = PreparedBase & {
    readonly kind: "server"
    readonly config: ServerConfigData
    readonly mods: readonly Mod[]
    readonly manifest: ManifestData
}

export type PreparedMod = PreparedBase & { readonly kind: "mod" }

export type Prepared = PreparedServer | PreparedMod

export type { ProjectKind }
