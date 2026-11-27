import { ref, watch } from "vue"
import type { BridgeHandle } from "../bridge/index.ts"
import type { GameStoreHandle } from "../game/store.ts"
import { Actions } from "../online/actions.ts"

/**
 * A friend's world asked for before the menu was up (shell/bridge/joins.cpp): Join from inside a
 * world, which left it first, or the launcher's Join (--join). Taken once the menu shows, and
 * joined once you're signed in and friends are read, the usual way (social.join), with the
 * entering screen after. `who`: whose world, while it's on the way; a no lands in the store's error.
 */
export function PendingJoin(opts: { bridge: BridgeHandle; store: GameStoreHandle }) {
    const actions = Actions({ bridge: opts.bridge })
    const who = ref<string | null>(null)
    let friend: { id: string; name: string } | null = null

    async function go() {
        if (!friend) return
        const status = opts.store.account?.status
        if (status === "signedOut" || status === "unreachable") {
            opts.store.error = `Sign in to join @${friend.name}.`
            friend = null
            who.value = null
            return
        }
        if (status !== "signedIn" || !opts.store.social?.view) return
        const going = friend
        friend = null
        await actions.joinFriend({ id: going.id, username: going.name, name: null })
        if (actions.problem.value) {
            opts.store.error = `Couldn't join @${going.name}: ${actions.problem.value}`
            who.value = null
        }
    }

    void opts.bridge.send("join.pending", {}).then((pending) => {
        friend = pending
        who.value = pending?.name ?? null
        void go()
    })
    watch(() => [opts.store.account?.status, !!opts.store.social?.view], () => { void go() })

    return {
        who: who,
    }
}
