import type { CompanionWire } from "./stars"
import type { BodyCausesWire, SystemBodyWire, SystemWire } from "../bridge"

/**
 * A planetary system as the player designs it: causes only. What each body
 * becomes — its air, temperature, field, seas, kind and history — comes from
 * terragen's body model (`system.preview`, see ./preview.ts), never from here.
 * planning/systems-and-bodies.md has the model.
 */
/** A body as the page holds it: the engine's shape (`SystemBodyWire`). */
export type Body = SystemBodyWire

export const STAR = "star" // what's selected when the star is

/** The causes the body model accepts (terragen/body.cpp: check), so sliders can't ask for a world that can't exist. */
export const LIMITS = {
    bodies: 32, // src/terragen/system.h MOST_BODIES
    age: { min: 0.002, max: 13 },
    mass: { min: -2.6, max: 2.6 },
    orbit: { star: { min: 0.05, max: 100 }, planet: { min: 1.5, max: 200 } },
    eccentricity: { min: 0, max: 0.9 },
    day: { min: 1, max: 6000 },
} as const

const COLORS = ["#8c9eaa", "#ba9678", "#87a99e", "#b3a090", "#aba4bf"]

/** A draft of a system, from one the engine gave (`system.home`: the one every world has today). */
export function System(from: SystemWire) {
    return {
        age: from.age,           // billions of years
        activity: from.activity, // how hard the star's wind blows
        selected: from.bodies.find((b) => b.playable)?.id ?? STAR,
        bodies: from.bodies.map((b): Body => ({ ...b })),
        made: 0,                 // bodies added so far, for names and ids
    }
}

export type SystemHandle = ReturnType<typeof System>

export function body(system: SystemHandle, id: string): Body {
    const found = system.bodies.find((b) => b.id === id)
    if (!found) throw new Error(`no body ${id} in the system`)
    return found
}

export function planets(system: SystemHandle) {
    return system.bodies.filter((b) => b.parent === null)
}

export function moonsOf(system: SystemHandle, id: string) {
    return system.bodies.filter((b) => b.parent === id)
}

/** The system as the body model takes it: parents before their moons, masses unlogged. */
export function causes(system: SystemHandle, star: { star: string; starMass: number; metallicity: number; age: number; wind: number; companion: CompanionWire }) {
    const ordered = [...planets(system), ...system.bodies.filter((b) => b.parent !== null)]
    return {
        star: star.star,
        starMass: star.starMass,
        metallicity: star.metallicity,
        age: star.age,
        activity: star.wind,
        companion: { ...star.companion },
        bodies: ordered.map((b): BodyCausesWire => ({
            id: b.id, parent: b.parent, orbit: b.orbit, eccentricity: b.eccentricity, mass: 10 ** b.mass,
            formed: b.formed, volatiles: b.volatiles, day: b.day, life: b.life, spin: b.spin,
        })),
    }
}

export function addPlanet(system: SystemHandle) {
    const n = ++system.made
    const id = `planet-${n}`
    const farthest = Math.max(0.4, ...planets(system).map((p) => p.orbit))
    system.bodies.push({
        id: id, name: `Planet ${n}`, parent: null, color: COLORS[n % COLORS.length]!, phase: (n * 0.173) % 1,
        playable: false, orbit: Math.min(LIMITS.orbit.star.max, +(farthest * 1.5).toFixed(2)), eccentricity: 0.03,
        inclination: 1, mass: -0.3, formed: Math.min(1, 0.3 + Math.log10(farthest * 1.5 + 1) * 0.6), volatiles: 0.4,
        day: 30, tilt: 10, life: false, spin: "free", land: null,
    })
    system.selected = id
}

export function addMoon(system: SystemHandle, parent: string) {
    if (body(system, parent).parent !== null) throw new Error("a moon needs a planet, not a moon")
    const n = ++system.made
    const id = `moon-${n}`
    const siblings = moonsOf(system, parent)
    const host = body(system, parent)
    system.bodies.push({
        id: id, name: `${host.name} ${["I", "II", "III", "IV", "V", "VI", "VII"][siblings.length] ?? siblings.length + 1}`,
        parent: parent, color: "#aabec5", phase: (n * 0.277) % 1, playable: false,
        orbit: Math.min(LIMITS.orbit.planet.max, 6 + siblings.length * 6), eccentricity: 0.01, inclination: 0,
        mass: -2, formed: host.formed, volatiles: 0.2, day: 100, tilt: 0, life: false, spin: "free", land: null,
    })
    system.selected = id
}

/** A second planet round a shared point: a moon as heavy as its host, close enough to fill its sky. */
export function addPartner(system: SystemHandle, parent: string) {
    const host = body(system, parent)
    if (host.parent !== null) throw new Error("a double needs a planet, not a moon")
    if (partnerOf(system, parent)) throw new Error(`${host.name} already has a partner`)
    const n = ++system.made
    const id = `moon-${n}`
    system.bodies.push({ ...host, id: id, name: `${host.name} B`, parent: parent, phase: (n * 0.277) % 1, playable: false, orbit: 10, eccentricity: 0.01, inclination: 0, land: null })
    system.selected = id
}

/**
 * The other half of a double planet, if it's in one: what makes one is that
 * the pair's balance point lies outside the planet, so a body's mass and
 * distance decide it, not a kind of body.
 */
export function partnerOf(system: SystemHandle, id: string): Body | null {
    const self = body(system, id)
    const outside = (moon: Body, host: Body) => moon.orbit * 10 ** moon.mass / (10 ** moon.mass + 10 ** host.mass) > 1
    if (self.parent !== null) {
        const host = body(system, self.parent)
        return outside(self, host) ? host : null
    }
    return moonsOf(system, id).find((moon) => outside(moon, self)) ?? null
}

/** Moves a body along its siblings: it takes the next one's orbit, as a place in the system is its distance. */
export function moveBody(system: SystemHandle, id: string, way: -1 | 1) {
    const moved = body(system, id)
    const siblings = system.bodies.filter((b) => b.parent === moved.parent).sort((a, z) => a.orbit - z.orbit)
    const other = siblings[siblings.indexOf(moved) + way]
    if (!other) return
    const orbit = moved.orbit
    moved.orbit = other.orbit
    other.orbit = orbit
}

/** The start body, or the planet it's a moon of: these stay, or there'd be nowhere to start. */
export const holdsStart = (system: SystemHandle, id: string) =>
    system.bodies.some((b) => b.playable && (b.id === id || b.parent === id))

export function removeBody(system: SystemHandle, id: string) {
    const gone = body(system, id)
    if (holdsStart(system, id)) throw new Error("the playable world can't be removed")
    system.bodies = system.bodies.filter((b) => b.id !== id && b.parent !== id)
    system.selected = gone.parent ?? STAR
}
