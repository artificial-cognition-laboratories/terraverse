import { ref } from "vue"
import { BridgeError, type BridgeHandle, type IntentKind, type Intents, type Player } from "../bridge/index.ts"

/**
 * What a page of the online screen does with people and places: join a world or a friend, answer
 * and send requests, invite, sign in and out. One at a time (`busy`); an expected no lands in
 * `problem` for the page to show, anything else throws. `joining`: whose world or which server
 * is being joined, for its button to say so.
 */
export function Actions(opts: { bridge: BridgeHandle }) {
    const busy = ref(false)
    const problem = ref<string | null>(null)
    const joining = ref<string | null>(null)

    async function send<K extends IntentKind>(kind: K, request: Intents[K]["request"]): Promise<boolean> {
        problem.value = null
        busy.value = true
        try {
            await opts.bridge.send(kind, request)
            return true
        } catch (e) {
            if (!(e instanceof BridgeError) || !e.refused) throw e
            problem.value = e.message
            return false
        } finally {
            busy.value = false
        }
    }
    async function joiningAs(id: string, run: () => Promise<boolean>) {
        joining.value = id
        try { await run() } finally { joining.value = null }
    }

    return {
        busy: busy,
        problem: problem,
        joining: joining,
        send: send,
        joinFriend: (player: Player) => joiningAs(player.id, () => send("social.join", { id: player.id, name: player.username })),
        /** From inside a world: set the join aside, then leave (saved as ever); the menu joins once it's up. */
        joinFromWorld: async (player: Player) => {
            if (await send("join.later", { id: player.id, name: player.username })) await send("game.leave", {})
        },
        joinServer: (server: { id: string; ref: string }) => joiningAs(server.id, () => send("servers.join", { ref: server.ref })),
        request: (username: string) => send("social.request", { username: username }),
        accept: (id: string) => send("social.accept", { id: id }),
        remove: (id: string) => send("social.remove", { id: id }),
        invite: (id: string) => send("social.invite", { id: id }),
        dismiss: (id: string) => send("social.dismiss", { id: id }),
    }
}

export type ActionsHandle = ReturnType<typeof Actions>
