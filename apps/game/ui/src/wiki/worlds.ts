// The wiki's Worlds pages: why the land is as it is. Each entry says what caused it, how to
// recognise it and what it gives you, and only what terragen really does (terragen/grid.cpp).

export type Lens = "land" | "worn" | "sediment" | "heavy"
export type WorldEntry = { id: string; group: string; title: string; lens: Lens; cause: string; recognise: string; gives: string }

export const WORLD_ENTRIES: WorldEntry[] = [
    { id: "valleys", group: "Water on the land", title: "Rivers cut valleys", lens: "worn",
        cause: "Running water wears its bed, harder the more of it has gathered and the steeper it falls. The biggest rivers cut first and furthest.",
        recognise: "Red shows what's been worn away. It follows the water and grows up its branches.",
        gives: "A valley is a road: follow water uphill and it leads to the range that feeds it." },
    { id: "ranges", group: "Water on the land", title: "Ranges hold their height", lens: "land",
        cause: "Where plates collide the land keeps being pushed up, while rivers keep wearing it down. A mountain stands where the two balance.",
        recognise: "Watch the high ground: it changes shape more than height. Old land with nothing pushing it up has worn low.",
        gives: "High, steep land means the plates are or were busy there, and that's where to look for veins." },
    { id: "plains", group: "Water on the land", title: "Plains are laid down", lens: "sediment",
        cause: "A stream carries only as much as its power allows. Where the land flattens it slows, and drops what it can't carry.",
        recognise: "Yellow shows what's been laid. Flat ground along a river's lower reach, and fans where it leaves the hills.",
        gives: "What rivers lay down makes silty, fertile soil: the best farmland is where they slowed." },
    { id: "shelf", group: "Water on the land", title: "The sea builds a shelf", lens: "sediment",
        cause: "What reaches the coast settles under the water, building up to just below the sea's level.",
        recognise: "Yellow under the blue at a river's mouth: shallows spreading out from where it meets the sea.",
        gives: "Shallow water runs out a long way off a big river's mouth, and deep water lies off coasts with none." },
    { id: "placers", group: "Water on the land", title: "Heavy grains settle first", lens: "heavy",
        cause: "Rivers wear gold and tin out of a range's core, where the veins are. Those grains are heavy and drop out sooner than the rest, so they gather where the river first slows.",
        recognise: "Purple marks the range's core and gold marks rich gravel just below it. Look on flat ground downstream of old mountains, not on the mountains.",
        gives: "Gold and tin you can reach without mining a vein, if you know which rivers to walk." },
]
