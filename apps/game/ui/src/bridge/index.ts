import { FailureCode, type EngineMessage, type IntentKind, type Intents } from "./types"

export type { Account, Friend, Invite, Player, Presence, Social, BodyCard, BodyCausesWire, BodyKind, CommunityCatalog, CommunityMod, CommunityTexture, CommunityWorld, CreativeItem, EngineMessage, FlightNear, FlightSpace, IntentKind, Intents, ItemInfo, LabControl, MakingOffer, LabScenario, LabValue, ControlIcon, ControlItem, ControlPage, ControlValue, MatterElement, MatterMixture, MatterReaction, MatterRock, MatterState, MatterStructure, MatterSubstance, Mode, PlanetariumBody, PlanetariumCell, PlanetariumMarker,
    PlanetariumFlight,
    PlanetariumHover, BodyLandWire, SystemBodyWire, SystemWire, UpdateCheck, PlanetariumLayer, PlanetariumMessage, PlanetariumPlace, PlanetariumViewport, PublicServer, MoveScene, MoveState, ReliefState, SceneKind, SandboxBlock, SandboxFrame, SandboxSpec, SceneRect, ScreenName, ServerDirectory, NodePos, SlotRef, Stack, StateSections, ToolId, BrushId, JointId, ActionId, Ask, UiEvent, UiEvents, UiValues, World } from "./types"
export { ACTION_IDS, BRUSH_IDS, FailureCode, RAIL_BRUSHES, TOOL_IDS } from "./types"

type Listener = (message: EngineMessage) => void

export class BridgeError extends Error {
    constructor(readonly kind: IntentKind, readonly code: number, message: string) {
        super(message)
    }

    /** The engine said no for a reason the player should read. */
    get refused() {
        return this.code === FailureCode.Refused
    }
}

/** The UI's only line to the engine: intents out, engine messages in. */
export function Bridge() {
    const listeners = new Set<Listener>()
    const query = window.terraQuery

    window.__terra = {
        receive: (message) => listeners.forEach((listener) => listener(message)),
    }

    return {
        connected: query !== undefined,
        send<K extends IntentKind>(kind: K, request: Intents[K]["request"]): Promise<Intents[K]["response"]> {
            if (!query) throw new Error("Bridge is not connected: not running inside the engine")
            return new Promise((resolve, reject) => {
                query({
                    request: JSON.stringify({ ...request, kind: kind }),
                    onSuccess: (response) => resolve(JSON.parse(response)),
                    onFailure: (code, message) => reject(new BridgeError(kind, code, message)),
                })
            })
        },
        on(listener: Listener) {
            listeners.add(listener)
            return () => listeners.delete(listener)
        },
    }
}

export type BridgeHandle = ReturnType<typeof Bridge>
