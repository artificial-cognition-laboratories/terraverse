import { invoke } from "@tauri-apps/api/core"
import { listen } from "@tauri-apps/api/event"
import type { Backend, Message } from "./types.ts"
import { Refused } from "./types.ts"

// The real backend: commands in src-tauri/src/lib.rs, and its messages on the
// "launcher" event. A failure arrives as { refused } (an expected no, worded for
// the player) or { bug } (thrown, never shown as an answer): src-tauri/src/failure.rs.

function isFailure(error: unknown): error is { refused: string } | { bug: string } {
    return typeof error === "object" && error !== null && ("refused" in error || "bug" in error)
}

async function call<T>(command: string, args: Record<string, unknown> = {}): Promise<T> {
    try {
        return await invoke<T>(command, args)
    } catch (error) {
        if (!isFailure(error)) throw error
        if ("refused" in error) throw new Refused(error.refused)
        throw new Error(`${command}: ${error.bug}`)
    }
}

export function TauriBackend(): Backend {
    return {
        about: () => call("about"),

        account: () => call("account"),
        signIn: () => call("sign_in"),
        cancelSignIn: () => call("cancel_sign_in"),
        retrySignIn: () => call("retry_sign_in"),
        signOut: () => call("sign_out"),

        social: () => call("social"),
        requestFriend: (username) => call("request_friend", { username }),
        acceptFriend: (id) => call("accept_friend", { id }),
        removeFriend: (id) => call("remove_friend", { id }),

        settings: () => call("settings"),
        setSettings: (settings) => call("set_settings", { settings }),

        refreshManifest: () => call("refresh_manifest"),
        cachedManifest: () => call("cached_manifest"),

        installed: () => call("installed"),
        install: (version) => call("install", { version }),
        cancel: (version) => call("cancel", { version }),
        remove: (version) => call("remove", { version }),

        game: () => call("game"),
        launch: (version) => call("launch", { version }),

        collectReport: (session) => call("collect_report", { session }),
        submitReport: (session, note) => call("submit_report", { session, note }),
        dismissCrash: () => call("dismiss_crash"),

        launcherUpdate: () => call("launcher_update"),
        updateLauncher: () => call("update_launcher"),

        openDataFolder: () => call("open_data_folder"),
        openUrl: (url) => call("open_url", { url }),

        listen(handler) {
            // Tauri subscribes asynchronously: an unlisten before then still takes effect
            let stopped = false
            let stop: (() => void) | null = null
            void listen<Message>("launcher", event => handler(event.payload)).then(unlisten => {
                if (stopped) unlisten()
                else stop = unlisten
            })
            return () => {
                stopped = true
                stop?.()
            }
        },
    }
}
