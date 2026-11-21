import type { BridgeHandle, RegistrySearch } from "../../bridge/index.ts"
import { useBridge } from "../../bridge/use.ts"

/** The ArcLabs mod registry, as the engine answers it (shell/bridge/registry.cpp). */
export function Registry(bridge: BridgeHandle) {
    return {
        front: () => bridge.send("registry.front", {}),
        search: (search: RegistrySearch) => bridge.send("registry.search", search),
        mod: (name: string) => bridge.send("registry.mod", { name: name }),
        file: (name: string, path: string) => bridge.send("registry.file", { name: name, path: path }),
    }
}
export type Registry = ReturnType<typeof Registry>

/** The registry, from the bridge a component is under. */
export function useRegistry(): Registry {
    return Registry(useBridge())
}
