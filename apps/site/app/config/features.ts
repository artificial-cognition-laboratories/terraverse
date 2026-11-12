/** The landing's pillars, each pointing at the docs that go deeper. */
export interface Feature {
    kicker: string
    title: string
    body: string
    to: string
}

export const features: Feature[] = [
    {
        kicker: "worlds",
        title: "Planets, not maps",
        body: "Plates collide into ranges, rain wears them into rivers, and climate decides what grows. Every world is a whole sphere, named by a code anyone can share.",
        to: "/docs/worlds",
    },
    {
        kicker: "matter",
        title: "Things are what they're made of",
        body: "Ore melts, wood burns and water boils away in thin air, because of what they are. Learn how the world works and it keeps working that way.",
        to: "/docs/matter",
    },
    {
        kicker: "machines",
        title: "Build what moves",
        body: "Parts, joints, wheels and engines, simulated as real rigid bodies. A cart, a crane or a car is something you engineer, not something you craft.",
        to: "/docs/machines",
    },
    {
        kicker: "modding",
        title: "A universe to make games in",
        body: "The official game is built from the same public Lua API you get. Switch it off and you have a clean universe for survival, racing or anything else.",
        to: "/docs/mods",
    },
]
