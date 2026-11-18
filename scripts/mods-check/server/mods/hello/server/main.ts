// A mod the check runs: content, timers, and a failing handler. Greeter builds on it.
content.blocks.create("hello:brick", { matter: "granite", texture: "brick.png" })
content.blocks.create("hello:lamp", { matter: "granite", texture: { top: "brick.png", sides: "brick.png" } })

export const greeting = (name: string) => `${mod.settings.greeting}, ${name}.`

console.log(`CHECK settings ${mod.name} ${mod.settings.greeting} loud=${mod.settings.loud}`)

let ticks = 0
game.time.every({ seconds: 0.5 }, () => { ticks++ })

game.time.after({ seconds: 1 }, () => {
    console.log(`CHECK after ${game.time.now.toFixed(1)} ticks=${ticks} players=${game.players.list().length}`)
    throw new Error("CHECK a handler that throws")
})
