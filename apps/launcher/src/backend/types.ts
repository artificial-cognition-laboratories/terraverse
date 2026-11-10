import type { Account, Social } from "@game/bridge/types.ts"

// The contract between the launcher's page and its backend (src-tauri/src/).
// Change both sides together. `mock.ts` fakes it for building the page in a
// browser; `tauri.ts` calls the real commands. The account and friends are the
// game's own types, so the game's account dropdown serves both.

export type { Account, Social }

export type Channel = "stable" | "beta"

/** One release, as the manifest describes it for this platform. */
export type Version = {
    id: string
    channel: Channel
    released: string // ISO date
    notes: string // a little markdown: paragraphs, **bold**, "- " lists
    size: number // bytes to download
    protocol: number | null // who can play with whom; in manifests from 0.1.3
}

export type Manifest = { versions: Version[]; fetched: number } // newest first

/** What's on disk: complete installs only; a half-done one is never listed. */
export type Installed = { id: string; size: number }

/** What the launcher remembers between runs. */
export type Settings = {
    beta: boolean // betas count as the newest, for Play when nothing's pinned
    pinned: string | null // play this version; null plays the newest
    onLaunch: "hide" | "close" | "stay"
    keepOld: boolean // false removes a version once a newer one has been played
}

/** The game, as the launcher sees it. */
export type Game =
    | { state: "idle" }
    | { state: "running"; version: string; since: number }
    | { state: "crashed"; version: string; session: string; code: number }

export type Report = { session: string; version: string; system: string; log: string; dump: boolean }

/** A newer launcher, ready to install (the launcher's own update, not a game version). */
export type LauncherUpdate = { version: string; notes: string }

export type About = { launcher: string; dataDir: string; platform: "linux-x86_64" | "windows-x86_64"; free: number }

/** Pushed by the backend while something's under way. */
export type Message =
    | { kind: "install"; version: string; received: number; total: number }
    | { kind: "installed"; version: string }
    | { kind: "installFailed"; version: string; reason: string }
    | { kind: "game"; game: Game }
    | { kind: "account"; account: Account }
    | { kind: "social"; social: Social | null }

export type Unlisten = () => void

export interface Backend {
    about(): Promise<About>

    /** ArcLabs, as in the game: a device code approved in the browser. Changes arrive as messages. */
    account(): Promise<Account>
    signIn(): Promise<void>
    cancelSignIn(): Promise<void>
    retrySignIn(): Promise<void>
    signOut(): Promise<void>

    /** Friends, as in the game; null until connected or when signed out. */
    social(): Promise<Social | null>
    requestFriend(username: string): Promise<void>
    acceptFriend(id: string): Promise<void>
    removeFriend(id: string): Promise<void>

    settings(): Promise<Settings>
    setSettings(settings: Settings): Promise<Settings>

    /** Refuses when offline; `cachedManifest` is what was last fetched. */
    refreshManifest(): Promise<Manifest>
    cachedManifest(): Promise<Manifest | null>

    installed(): Promise<Installed[]>
    install(version: string): Promise<void> // progress arrives as messages
    cancel(version: string): Promise<void>
    remove(version: string): Promise<void>

    game(): Promise<Game>
    launch(version: string): Promise<void>

    collectReport(session: string | null): Promise<Report> // null: the last session, sent by hand
    submitReport(session: string, note: string): Promise<void>
    dismissCrash(): Promise<void>

    /** Null when this launcher is the newest, or the check couldn't be made. */
    launcherUpdate(): Promise<LauncherUpdate | null>
    /** Installs it and restarts; refuses while the game runs. */
    updateLauncher(): Promise<void>

    openDataFolder(): Promise<void>
    openUrl(url: string): Promise<void>

    listen(handler: (message: Message) => void): Unlisten
}

/** An expected no, worded for the player. */
export class Refused extends Error {}
