/**
 * Starting set-ups for the Reactor: what goes in the vessel, and what it
 * sits in. Which reactions run, and how far, is the engine's business.
 */
export type Preset = {
    id: string
    label: string
    hint: string
    contents: Record<string, number> // moles
    surroundings: number             // kelvin
    pressure: number                 // pascals
    power: number                    // watts of current
    light: number                    // watts of light
}

const AIR = (oxygen: number) => ({ oxygen: oxygen, nitrogen: oxygen * 3.76 })

export const PRESETS: Preset[] = [
    { id: "gas", label: "Gas flame", hint: "Methane in air. Nothing happens until you strike a spark.",
        contents: { methane: 1, ...AIR(2.5) }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "charcoal", label: "Charcoal fire", hint: "Charcoal short of air: once lit, it makes carbon monoxide as well as CO₂.",
        contents: { carbon: 3, ...AIR(1.5) }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "bloomery", label: "Iron furnace", hint: "Iron ore and charcoal in a furnace held at red heat: carbon monoxide strips the ore to iron.",
        contents: { hematite: 1, carbon: 4, ...AIR(1) }, surroundings: 1300, pressure: 101325, power: 0, light: 0 },
    { id: "copper", label: "Copper smelt", hint: "Green malachite roasts to black oxide, then carbon monoxide turns it to copper.",
        contents: { malachite: 1, carbon_monoxide: 3 }, surroundings: 1100, pressure: 101325, power: 0, light: 0 },
    { id: "kiln", label: "Lime kiln", hint: "Limestone held above 1100 K gives up its CO₂ and leaves quicklime.",
        contents: { calcite: 2 }, surroundings: 1300, pressure: 101325, power: 0, light: 0 },
    { id: "cell", label: "Electrolysis cell", hint: "Current through water: hydrogen and oxygen, two to one. Turn the power up.",
        contents: { water: 10 }, surroundings: 293, pressure: 101325, power: 2000, light: 0 },
    { id: "rocket", label: "Hydrogen and oxygen", hint: "What electrolysis made, put back together. Strike a spark.",
        contents: { hydrogen: 2, oxygen: 1 }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "nail", label: "Rusting nail", hint: "Iron, air and water at room temperature. Slow: try ×1000.",
        contents: { iron: 4, oxygen: 3, water: 5 }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "roast", label: "Roasting copper ore", hint: "Chalcopyrite in a hot draught: its sulfur burns off as choking fumes, leaving copper and iron oxides.",
        contents: { chalcopyrite: 2, ...AIR(7) }, surroundings: 900, pressure: 101325, power: 0, light: 0 },
    { id: "tin", label: "Tin smelt", hint: "Cassiterite from river gravel and carbon monoxide at a bright red heat.",
        contents: { cassiterite: 1, carbon_monoxide: 3 }, surroundings: 1250, pressure: 101325, power: 0, light: 0 },
    { id: "lead", label: "Lead smelt", hint: "Galena roasted in air to lead oxide, which a smoky fire turns to lead.",
        contents: { galena: 2, carbon: 3, ...AIR(4) }, surroundings: 1000, pressure: 101325, power: 0, light: 0 },
    { id: "pottery", label: "Pottery kiln", hint: "Clay above about 800 K gives up its water and becomes pottery for good.",
        contents: { kaolinite: 2 }, surroundings: 1000, pressure: 101325, power: 0, light: 0 },
    { id: "sulfur", label: "Burning sulfur", hint: "Yellow crystals from a volcanic vent: a blue flame once lit.",
        contents: { sulfur: 2, ...AIR(3) }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "bronze", label: "Bronze casting", hint: "Copper and tin in a crucible: once both are molten they stir into bronze, which melts lower than copper.",
        contents: { copper: 15, tin: 1 }, surroundings: 1450, pressure: 101325, power: 0, light: 0 },
    { id: "steel", label: "Steel from cast iron", hint: "Air blown through molten cast iron burns off most of its carbon: steel, and a lot of carbon monoxide.",
        contents: { cast_iron: 5, ...AIR(9) }, surroundings: 1650, pressure: 101325, power: 0, light: 0 },
    { id: "charcoal_kiln", label: "Charcoal kiln", hint: "Wood heated with too little air to burn: it chars to charcoal instead. Give it more air and it burns away.",
        contents: { wood: 3, ...AIR(0.3) }, surroundings: 700, pressure: 101325, power: 0, light: 0 },
    { id: "campfire", label: "Campfire", hint: "Wood in open air. Strike a spark.",
        contents: { wood: 1, ...AIR(8) }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "glass", label: "Glass furnace", hint: "Sand and soda ash above about 1100 K melt into glass.",
        contents: { silica: 2, soda_ash: 2 }, surroundings: 1400, pressure: 101325, power: 0, light: 0 },
    { id: "mortar", label: "Lime mortar", hint: "Quicklime and water hiss into slaked lime, which takes CO₂ back from the air over days. Try ×1000.",
        contents: { lime: 2, water: 3, carbon_dioxide: 3 }, surroundings: 293, pressure: 101325, power: 0, light: 0 },
    { id: "leaf", label: "Leaf in sunlight", hint: "Light builds sugar from CO₂ and water and gives off oxygen.",
        contents: { carbon_dioxide: 6, water: 10 }, surroundings: 293, pressure: 101325, power: 0, light: 300 },
]
