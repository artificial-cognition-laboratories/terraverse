// The time where you stand, and skipping the universe forward. There's no setting the time of
// day: with a real sky it's where the planet is in its turn, so a skip moves every world on
// together, and a night skipped is a night that passed.
//
// Times are local. It's noon on one side of a world and midnight on the other, so /time skip
// counts from where whoever typed it stands: /time dawn is their dawn, /time 0600 their six in
// the morning, /time skip 6h six of their world's hours. The console stands nowhere, so it can't skip.

const MOMENTS: readonly Moment[] = ["dawn", "noon", "dusk", "midnight"]
// Times of day by name, at fixed hours. dawn and dusk are the sun's, so they move with the season.
const NAMED: Record<string, number> = { morning: 9, day: 12, afternoon: 15, evening: 19, night: 22 }

game.commands.create("time", {
    description: "The time where you stand. With a time (0600, noon, dawn, afternoon), skip the universe forward to it",
    args: {
        time: {
            type: "string",
            optional: true,
            description: "A time to skip to: 0600 style, or dawn, morning, noon, afternoon, dusk, evening, night, midnight",
            suggest: [
                { value: "dawn", description: "When the sun rises where you stand" },
                { value: "morning", description: "09:00" },
                { value: "noon", description: "12:00, the sun at its highest" },
                { value: "afternoon", description: "15:00" },
                { value: "dusk", description: "When the sun sets where you stand" },
                { value: "evening", description: "19:00" },
                { value: "night", description: "22:00" },
                { value: "midnight", description: "00:00" },
                { value: "0600", description: "Any time, 24-hour: 0600 is six in the morning" },
            ],
        },
    },
    async run({ player, args, reply }) {
        const you = here(player)
        const now = you.world.calendar.at(you.position)
        if (args.time === undefined) {
            reply(`${clock(now.hour)}, day ${now.day} of year ${now.year}, ${now.season}. ${sunlight(now)}`)
            return
        }
        if (!you.can("time:skip")) throw new Refused("You may not skip time: it needs time:skip.")
        await skip(you, now, until(now, args.time.toLowerCase()), reply)
    },
})

game.commands.create("time skip", {
    description: "Move the whole universe forward by your world's hours: 6h, 30m, 2d",
    args: {
        by: {
            type: "string",
            description: "How far: your world's hours, minutes or days",
            suggest: [
                { value: "1h", description: "An hour" },
                { value: "6h", description: "Six hours" },
                { value: "30m", description: "Half an hour" },
                { value: "1d", description: "A whole day" },
            ],
        },
    },
    permission: "time:skip",
    async run({ player, args, reply }) {
        const you = here(player)
        const now = you.world.calendar.at(you.position)
        await skip(you, now, by(now, args.by.toLowerCase()), reply)
    },
})

/** Skip, then tell everyone, from where whoever asked stands. */
async function skip(you: Player, now: CalendarReading, seconds: number, reply: (text: string) => void) {
    await game.time.skip({ seconds: seconds })
    const later = you.connected ? you.world.calendar.at(you.position) : null
    const hours = now.dayLength ? Math.round(seconds / now.dayLength * 24) : 0
    game.announce(`${you.name} skipped ${hours === 1 ? "an hour" : `${hours} hours`} on.` +
        (later ? ` It's ${clock(later.hour)} on ${you.world.name}.` : ""))
    reply(later ? `${clock(later.hour)}, day ${later.day}.` : "Skipped.")
}

/** Whoever typed it, standing somewhere. */
function here(player: Player | undefined): Player {
    if (!player) throw new Refused("The console isn't anywhere, so it has no time of day: run /time in the game.")
    return player
}

/** Game seconds to the next time of day `to` names here: 0600, a moment, or a named hour. */
function until(now: CalendarReading, to: string): number {
    if (now.dayLength === null) throw new Refused("The sun doesn't move here: there's no time of day to skip to.")
    const military = /^([01]?\d|2[0-3])([0-5]\d)$/.exec(to)
    const hour = military ? Number(military[1]) + Number(military[2]) / 60
        : Object.hasOwn(NAMED, to) ? NAMED[to]!
        : MOMENTS.includes(to as Moment) ? (to as Moment)
        : null
    if (hour === null) throw new Refused(`A time is 0600 style, or dawn, morning, noon, afternoon, dusk, evening, night or midnight. Not "${to}".`)
    const seconds = now.until(hour)
    if (seconds === null) throw new Refused(to === "dawn" || to === "dusk"
        ? `The sun doesn't ${to === "dawn" ? "rise" : "set"} here today.`
        : `There's no ${to} here.`)
    return seconds
}

/** Game seconds in `by` of this world's hours, minutes or days. */
function by(now: CalendarReading, by: string): number {
    if (now.dayLength === null) throw new Refused("The sun doesn't move here: there's no time of day to skip through.")
    const amount = /^(\d+(?:\.\d+)?)([mhd])$/.exec(by)
    if (!amount) throw new Refused(`Skip by hours, minutes or days: 6h, 30m, 2d. Not "${by}". For a time of day: /time 0600.`)
    const hours = Number(amount[1]) * { m: 1 / 60, h: 1, d: 24 }[amount[2] as "m" | "h" | "d"]
    const seconds = hours / 24 * now.dayLength
    if (seconds < 1) throw new Refused("That's less than a moment.")
    if (seconds > 24 * 3600) throw new Refused(`That's too far at once: at most ${Math.floor(24 * 3600 / now.dayLength)} days here.`)
    return seconds
}

/** 0–24 → "06:30". */
function clock(hour: number): string {
    // to the nearest minute: the engine's clock moves in steps of a few seconds
    const minutes = Math.round(hour * 60) % (24 * 60)
    return `${String(Math.floor(minutes / 60)).padStart(2, "0")}:${String(minutes % 60).padStart(2, "0")}`
}

/** When the sun's up today, here. */
function sunlight(now: CalendarReading): string {
    if (now.dayLength === null) return now.daylight > 0 ? "The sun stands still overhead." : "The sun never rises here."
    if (now.daylight >= 1) return "The sun won't set today."
    if (now.daylight <= 0) return "The sun won't rise today."
    return `The sun's up ${clock(12 * (1 - now.daylight))}–${clock(12 * (1 + now.daylight))}.`
}
