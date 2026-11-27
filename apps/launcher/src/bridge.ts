import { BridgeError, FailureCode, type BridgeHandle, type IntentKind, type Intents } from "@game/bridge/index.ts"
import { Refused, type Backend } from "./backend/index.ts"

type Answer<K extends IntentKind> = (request: Intents[K]["request"]) => Promise<Intents[K]["response"]>
type Answers = { [K in IntentKind]?: Answer<K> }

/**
 * The game's bridge, as the launcher answers it: the intents the game's account corner and
 * friends popover send (friends/Friends.vue), each one the backend's call of the same meaning, so
 * one popover serves both. Anything else, and what only a running game can do (inviting, joining),
 * is refused with the reason, as the engine refuses what it can't do.
 */
/** `version`: what Play would launch now, which Play as launches too (store.target) */
export function LauncherBridge(opts: { backend: Backend; version: () => string | null }) {
    const b = opts.backend
    const none = async () => null
    const answers: Answers = {
        "account.signIn": () => b.signIn().then(none),
        "account.cancel": () => b.cancelSignIn().then(none),
        "account.retry": () => b.retrySignIn().then(none),
        "account.signOut": () => b.signOut().then(none),
        "account.leave": () => b.leave().then(none),
        "profiles.list": () => b.profiles(),
        "profiles.use": (r) => b.useProfile(r.name).then(none),
        "profiles.forget": (r) => b.forgetProfile(r.name).then(none),
        // the game opens and joins them once it's signed in (its menu takes --join)
        "social.join": async (r) => {
            const version = opts.version()
            if (!version) throw new Refused("Choose a version to play first.")
            await b.launchJoin(version, r.id, r.name)
            return null
        },
        "profiles.playAs": async (r) => {
            const version = opts.version()
            if (!version) throw new Refused("Choose a version to play first.")
            await b.launchAs(version, r.name)
            return null
        },
        "social.request": (r) => b.requestFriend(r.username).then(none),
        "social.accept": (r) => b.acceptFriend(r.id).then(none),
        "social.remove": (r) => b.removeFriend(r.id).then(none),
        open: (r) => b.openUrl(r.url).then(none),
    }
    const inGame: Partial<Record<IntentKind, string>> = {
        "social.invite": "Inviting a friend",
        "social.dismiss": "Dismissing an invite",
    }

    async function send<K extends IntentKind>(kind: K, request: Intents[K]["request"]): Promise<Intents[K]["response"]> {
        const answer = answers[kind] as Answer<K> | undefined
        if (!answer) {
            const what = inGame[kind]
            throw new BridgeError(kind, FailureCode.Refused, what ? `${what} happens in the game: start it first.` : `The launcher doesn't answer ${kind}.`)
        }
        try {
            return await answer(request)
        } catch (error) {
            if (error instanceof Refused) throw new BridgeError(kind, FailureCode.Refused, error.message)
            throw error
        }
    }

    return {
        connected: true,
        send: send,
        // the launcher's messages reach its store (store.ts), which hands the popover its state
        on: () => () => false,
    } satisfies BridgeHandle
}
