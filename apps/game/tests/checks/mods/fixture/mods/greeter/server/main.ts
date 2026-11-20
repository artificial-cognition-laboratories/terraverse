// Builds on hello: its import is hello's own loaded module, not a copy. Also crashes the
// sidecar once, so the check sees it restarted (nothing imports greeter, so the crash's
// fixture-only types stay here).
import { existsSync, writeFileSync } from "node:fs"
import { greeting } from "@check/hello"

console.log(`CHECK greeter loaded: ${greeting("check")}`)
// each mod has its own `mod`: greeter here, and hello inside greeting() above, called from here
console.log(`CHECK greeter is ${mod.name}`)
game.players.on("join", player => player.tell(greeting(player.name)))

game.time.after({ seconds: 1.5 }, async () => {
    await game.time.wait({ seconds: 0.5 })
    console.log("CHECK waited")
    const marker = `${import.meta.dir}/../../crashed`
    if (!existsSync(marker)) {
        writeFileSync(marker, "")
        console.log("CHECK crashing")
        process.exit(3)
    }
    console.log("CHECK alive after restart")
})
