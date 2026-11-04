import { Planet } from "./planet.js"

const canvas = document.getElementById("planet")
const planet = Planet(canvas, () => canvas.classList.add("ready"))
if (!planet) document.body.classList.add("no-webgl")

// A different real world each visit, named by its code.
const worlds = await (await fetch("worlds/worlds.json")).json()
const world = worlds[Math.floor(Math.random() * worlds.length)]
document.getElementById("hero-code").textContent = world.code
planet?.show(world.name)

for (const box of document.querySelectorAll("[data-copy]")) {
    const button = box.querySelector("[data-copy-button]")
    button.addEventListener("click", async () => {
        try {
            await navigator.clipboard.writeText(box.querySelector("code").textContent)
            button.textContent = "copied"
        } catch {
            button.textContent = "select it"
        }
        setTimeout(() => { button.textContent = "copy" }, 1600)
    })
}
