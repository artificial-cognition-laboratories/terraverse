import { inject, type InjectionKey } from "vue"
import type { BridgeHandle } from "."

export const BRIDGE: InjectionKey<BridgeHandle> = Symbol("bridge")

export function useBridge() {
    const bridge = inject(BRIDGE)
    if (!bridge) throw new Error("useBridge() outside the app: the bridge was not provided")
    return bridge
}
