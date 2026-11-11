import type { PlanetariumShot } from "../bridge"

export type Screen = "home" | "worlds" | "create" | "settings" | "community" | "wiki" | "attributions"

// The composed view behind each screen (src/planetarium/shot.h). The screens
// that don't show a planet of their own keep the home shot, so the camera holds
// still between them; Play turns to its own, and Create keeps it (its own
// planet in the same place) till another body's chosen. null: the screen's own view.
export const SHOT: Record<Screen, PlanetariumShot | null> = {
    home: "home",
    worlds: "play",
    create: "play",
    settings: "home",
    community: "home",
    wiki: "home",
    attributions: "home",
}
