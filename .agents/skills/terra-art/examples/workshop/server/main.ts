// The server half: runs once as the universe loads, then reacts to what happens.
// What's available here: https://terraverse.arclabs.it/docs/mods/server
import * as pic from "./art"

content.blocks.create("workshop:bench", { matter: "wood", share: 0.5, description: "Workbench", texture: { top: pic.benchTop, bottom: pic.planks, sides: pic.benchSide } })
content.blocks.create("workshop:planks", { matter: "wood", share: 0.6, texture: pic.planks })
content.blocks.create("workshop:stone_brick", { matter: "granite", texture: pic.stoneBrick })
content.blocks.create("workshop:log", { matter: "wood", texture: { top: pic.logEnd, bottom: pic.logEnd, sides: pic.logSide } })

content.items.create("workshop:stone_axe", { icon: pic.stoneAxe })
content.items.create("workshop:flint_chisel", { icon: pic.flintChisel })
content.items.create("workshop:stone_pick", { icon: pic.stonePick })

