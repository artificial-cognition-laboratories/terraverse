import { err } from "../../../errors"
import type { ChannelT } from "../../channel"
import { Call } from "../../wire"
import { checks } from "../checks"

export type Vec3 = readonly [number, number, number]
export type Season = "spring" | "summer" | "autumn" | "winter"
export type Moment = "dawn" | "noon" | "dusk" | "midnight"

/** A place's time on its world, as it was when read. */
export type CalendarReading = {
    readonly year: number
    readonly day: number
    readonly hour: number
    readonly season: Season
    readonly daylight: number
    readonly dayLength: number | null
    until(when: Moment | number): number | null
}

export type World = {
    readonly name: string
    readonly calendar: { at(pos: Vec3): CalendarReading }
}

const SEASONS: readonly string[] = ["spring", "summer", "autumn", "winter"]

/**
 * Worlds — the planets as mods hold them, by name. So far only a world's calendar is wired:
 * the rest of World (blocks, matter, bodies) says so when it's touched.
 */
export function Worlds(opts: { channel: ChannelT }) {
    const held = new Map<string, World>()

    /** The world a place is on, and its time there, in one call. */
    function where(pos: Vec3): { world: World; reading: CalendarReading } {
        const raw = parse.time(opts.channel.ask(Call["world.time"], pos[0], pos[1], pos[2]))
        if (!raw) throw err("NOT_PLACED", { detail: `${JSON.stringify(pos)} is on no world (a seam, or a world terra didn't make)` })
        return { world: of(raw.body), reading: raw.reading }
    }

    function of(name: string): World {
        const known = held.get(name)
        if (known) return known
        const world: World = Object.freeze({
            name: name,
            calendar: Object.freeze({
                at(pos: Vec3): CalendarReading {
                    const here = where(checks.vec3(pos))
                    if (here.world !== world) throw err("WRONG_WORLD", { detail: `${JSON.stringify(pos)} is on ${here.world.name}, not ${name}` })
                    return here.reading
                },
            }),
            get planet() { return checks.unwired("world.planet") },
            get players() { return checks.unwired("world.players") },
            get spawn() { return checks.unwired("world.spawn") },
            get matter() { return checks.unwired("world.matter") },
            get bodies() { return checks.unwired("world.bodies") },
            get store() { return checks.unwired("world.store") },
            get weather() { return checks.unwired("world.weather") },
            get: () => checks.unwired("world.get"),
            find: () => checks.unwired("world.find"),
            set: () => checks.unwired("world.set"),
            fill: () => checks.unwired("world.fill"),
            replace: () => checks.unwired("world.replace"),
            regenerate: () => checks.unwired("world.regenerate"),
            copy: () => checks.unwired("world.copy"),
            paste: () => checks.unwired("world.paste"),
            keep: () => checks.unwired("world.keep"),
            guard: () => checks.unwired("world.guard"),
            drop: () => checks.unwired("world.drop"),
            on: () => checks.unwired("world.on"),
            climate: () => checks.unwired("world.climate"),
        })
        held.set(name, world)
        return world
    }

    return { where: where }
}

export type WorldsT = ReturnType<typeof Worlds>

const parse = {
    /** [body, year, day, hour, daylight, season, day seconds], or nil: off every world. */
    time(answer: unknown): { body: string; reading: CalendarReading } | null {
        if (answer === null || answer === undefined) return null
        if (!Array.isArray(answer) || answer.length !== 7) throw err("PROTOCOL", { detail: "world.time didn't answer a reading" })
        const [body, year, day, hour, daylight, season, seconds] = answer as unknown[]
        if (typeof body !== "string" || typeof season !== "string" || !SEASONS.includes(season)
            || ![year, day, hour, daylight, seconds].every(n => typeof n === "number" && Number.isFinite(n)))
            throw err("PROTOCOL", { detail: `world.time answered ${JSON.stringify(answer)}` })
        return {
            body: body,
            reading: Reading({
                year: year as number, day: day as number, hour: hour as number, daylight: daylight as number,
                season: season as Season, dayLength: (seconds as number) > 0 ? seconds as number : null,
            }),
        }
    },
}

/** One reading, and when a moment of its day next comes: worked out here, from the sun's day. */
function Reading(r: Omit<CalendarReading, "until">): CalendarReading {
    // the sun's up for `daylight` of the day, either side of noon
    const MOMENTS: Record<Moment, number | null> = {
        midnight: 0,
        noon: 12,
        dawn: r.daylight > 0 && r.daylight < 1 ? 12 * (1 - r.daylight) : null,
        dusk: r.daylight > 0 && r.daylight < 1 ? 12 * (1 + r.daylight) : null,
    }

    function until(when: Moment | number): number | null {
        let hour: number | null
        if (typeof when === "number") {
            if (!Number.isFinite(when) || when < 0 || when >= 24) throw err("ARGUMENT_INVALID", { detail: `an hour is 0 to 24, not ${when}` })
            hour = when
        } else {
            if (!(when in MOMENTS)) throw err("ARGUMENT_INVALID", { detail: `the moments are dawn, noon, dusk and midnight, not ${JSON.stringify(when)}` })
            hour = MOMENTS[when]
        }
        if (hour === null || r.dayLength === null) return null
        // the next one: now counts as a day away
        let hours = ((hour - r.hour) % 24 + 24) % 24
        if (hours < 1e-6) hours = 24
        return hours / 24 * r.dayLength
    }

    return Object.freeze({ ...r, until: until })
}
