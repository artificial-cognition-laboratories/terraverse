// The star system a new server starts with: a fresh seed, laid out as the game lays out a
// new world. Encoded by the game's own encoder (apps/game/ui), so a new server's code is
// always this build's, never a version that went stale.
import { Companion } from "../../../../../game/ui/src/system/stars"
import { DEFAULTS, encodeWorldCode, NEW_WORLD_SIZE, NEW_WORLD_STYLE, randomSeed } from "../../../../../game/ui/src/worldgen/config"

/** A new world code, as the game's create screen would make with nothing changed. */
export function freshSystem(): string {
    return encodeWorldCode({ ...DEFAULTS, companion: Companion(), size: NEW_WORLD_SIZE, style: NEW_WORLD_STYLE, seed: randomSeed() })
}
