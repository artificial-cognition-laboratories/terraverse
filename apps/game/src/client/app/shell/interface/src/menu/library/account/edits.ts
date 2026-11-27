import { ref } from "vue"
import { BridgeError, type BridgeHandle, type IntentKind, type Intents } from "../../../bridge/index.ts"

/**
 * The account page's changes, sent one at a time (`busy`): the answer back, or null with the
 * backend's reason in `problem` for the page to show. Anything but a refusal throws.
 */
export function Edits(opts: { bridge: BridgeHandle }) {
    const busy = ref(false)
    const problem = ref<string | null>(null)

    async function send<K extends IntentKind>(kind: K, request: Intents[K]["request"]): Promise<Intents[K]["response"] | null> {
        problem.value = null
        busy.value = true
        try {
            return await opts.bridge.send(kind, request)
        } catch (e) {
            if (!(e instanceof BridgeError) || !e.refused) throw e
            problem.value = e.message
            return null
        } finally {
            busy.value = false
        }
    }

    return {
        busy: busy,
        problem: problem,
        send: send,
    }
}

export type EditsHandle = ReturnType<typeof Edits>
