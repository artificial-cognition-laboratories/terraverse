import { computed, reactive, readonly, shallowRef } from "vue"
import type { InjectionKey } from "vue"
import type { About, Account, Backend, LauncherUpdate, Game, Installed, Manifest, Settings, Social, Version } from "./backend/index.ts"
import { Refused } from "./backend/index.ts"

// What the page knows, all from the backend. `null` is "not told yet": render
// nothing rather than guess. Actions call the backend and take its answer;
// nothing is updated optimistically. A refusal lands in `refusal` for the
// page to show; anything else is a bug and stays thrown.

export type Download = { received: number; total: number }

/** Where one version stands, for the screens to switch on. */
export type Standing =
    | { state: "absent" }
    | { state: "downloading"; progress: Download }
    | { state: "failed"; reason: string }
    | { state: "installed"; size: number }

type State = {
    about: About | null
    settings: Settings | null
    manifest: Manifest | null
    offline: string | null // why the manifest couldn't be refreshed
    installed: Installed[] | null
    downloads: Record<string, Download>
    failures: Record<string, string>
    game: Game | null
    playAfterInstall: string | null // Play on a version not yet installed: install, then launch
    refusal: string | null
    launcherUpdate: LauncherUpdate | null
}

export function LauncherStore({ backend }: { backend: Backend }) {
    const state = reactive<State>({
        about: null, settings: null, manifest: null, offline: null, installed: null,
        downloads: {}, failures: {}, game: null, playAfterInstall: null, refusal: null, launcherUpdate: null,
    })

    // the game's account and friends, kept as given: the game's dropdown takes them as they are
    const account = shallowRef<Account | null>(null)
    const social = shallowRef<Social | null>(null)

    backend.listen(message => {
        switch (message.kind) {
            case "install":
                state.downloads[message.version] = { received: message.received, total: message.total }
                break
            case "installed":
                delete state.downloads[message.version]
                void refreshInstalled().then(() => {
                    if (state.playAfterInstall !== message.version) return
                    state.playAfterInstall = null
                    return attempt(() => backend.launch(message.version))
                })
                break
            case "installFailed":
                delete state.downloads[message.version]
                state.failures[message.version] = message.reason
                if (state.playAfterInstall === message.version) state.playAfterInstall = null
                break
            case "game":
                state.game = message.game
                // playing a newer version can remove older ones (Settings: keep old versions)
                void refreshInstalled()
                break
            case "account":
                account.value = message.account
                break
            case "social":
                social.value = message.social
                break
        }
    })

    async function attempt<T>(action: () => Promise<T>): Promise<T | undefined> {
        state.refusal = null
        try {
            return await action()
        } catch (error) {
            if (!(error instanceof Refused)) throw error
            state.refusal = error.message
            return undefined
        }
    }

    async function refreshInstalled() {
        state.installed = await backend.installed()
    }

    async function refreshManifest() {
        try {
            state.manifest = await backend.refreshManifest()
            state.offline = null
        } catch (error) {
            if (!(error instanceof Refused)) throw error
            state.offline = error.message
            state.manifest = await backend.cachedManifest()
        }
    }

    async function load() {
        const [about, given, friends, settings, game] = await Promise.all([
            backend.about(), backend.account(), backend.social(), backend.settings(), backend.game(),
            refreshManifest(), refreshInstalled()])
        state.about = about
        account.value = given
        social.value = friends
        state.settings = settings
        state.game = game
        // after the rest: a slow update server mustn't hold the page up
        state.launcherUpdate = await backend.launcherUpdate()
    }

    /** Every version is offered; betas count as the newest only when asked for. */
    const betaLatest = computed(() => !!state.settings?.beta)
    const visible = computed<Version[]>(() => state.manifest?.versions ?? [])

    /** What Play launches: the pinned version, else the newest (stable, unless betas are asked for). */
    const target = computed<Version | null>(() => {
        const pinned = state.settings?.pinned
        if (pinned) return visible.value.find(v => v.id === pinned) ?? null
        return visible.value.find(v => v.channel === "stable" || betaLatest.value) ?? null
    })

    function standing(id: string): Standing {
        const download = state.downloads[id]
        if (download) return { state: "downloading", progress: download }
        const failure = state.failures[id]
        if (failure) return { state: "failed", reason: failure }
        const install = state.installed?.find(i => i.id === id)
        return install ? { state: "installed", size: install.size } : { state: "absent" }
    }

    const used = computed(() => state.installed?.reduce((sum, i) => sum + i.size, 0) ?? null)

    async function install(id: string) {
        delete state.failures[id]
        await attempt(() => backend.install(id))
    }

    async function changeSettings(change: Partial<Settings>) {
        const current = state.settings
        if (!current) throw new Error("settings changed before they were loaded")
        const answer = await attempt(() => backend.setSettings({ ...current, ...change }))
        if (answer) state.settings = answer
    }

    return {
        state: readonly(state),
        account: computed(() => account.value),
        social: computed(() => social.value),
        betaLatest,
        visible,
        target,
        used,
        standing,
        load,
        refreshManifest: () => attempt(refreshManifest),
        dismissRefusal() { state.refusal = null },

        signIn: () => attempt(() => backend.signIn()),
        cancelSignIn: () => attempt(() => backend.cancelSignIn()),
        retrySignIn: () => attempt(() => backend.retrySignIn()),
        signOut: () => attempt(() => backend.signOut()),
        requestFriend: (username: string) => attempt(() => backend.requestFriend(username)),
        acceptFriend: (id: string) => attempt(() => backend.acceptFriend(id)),
        removeFriend: (id: string) => attempt(() => backend.removeFriend(id)),
        /** What the game's dropdown offers that only the game can do yet. */
        notInLauncher(what: string) { state.refusal = `${what} from the launcher isn't wired yet: do it in the game.` },

        changeSettings,
        pin: (id: string | null) => changeSettings({ pinned: id }),

        install,
        async cancel(id: string) {
            if (state.playAfterInstall === id) state.playAfterInstall = null
            await attempt(() => backend.cancel(id))
            delete state.downloads[id]
        },
        async remove(id: string) {
            await attempt(() => backend.remove(id))
            await refreshInstalled()
            if (state.settings?.pinned === id) await changeSettings({ pinned: null })
        },
        /** Launch, installing first if it isn't there yet. */
        async play(id: string) {
            if (standing(id).state === "installed") return void await attempt(() => backend.launch(id))
            state.playAfterInstall = id
            await install(id)
        },

        collectReport: (session: string | null) => backend.collectReport(session),
        submitReport: (session: string, note: string) => attempt(() => backend.submitReport(session, note)),
        dismissCrash: () => attempt(() => backend.dismissCrash()),
        updateLauncher: () => attempt(() => backend.updateLauncher()),
        openDataFolder: () => attempt(() => backend.openDataFolder()),
        openUrl: (url: string) => attempt(() => backend.openUrl(url)),
    }
}

export type LauncherStore = ReturnType<typeof LauncherStore>
export const STORE: InjectionKey<LauncherStore> = Symbol("launcher store")
