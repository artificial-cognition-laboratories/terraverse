# Blocks

A block is a cube; each face is a 16 × 16 texture. `texture: t` puts one on every face;
`texture: { top, bottom?, sides }` gives the top and bottom their own (bottom defaults to the
sides). The inventory and the sheet show the top and two sides, so the top and side carry the
look; the bottom is seen least.

## Two kinds of block, and which layers they want

**Natural** (stone, soil, sand, ore, bark): the face must tile with no visible edge.
- `noise` with **two or three close colours**, `scale` 6–8. Bigger contrast or a small scale
  makes stripes and diagonals that line up across blocks: the tiled patch shows it at once.
- No `border`, no `bevel`: both draw an edge on every block, so the ground looks like a grid.
- `grain` for bark and fibrous things, along "y" (bark runs up the trunk).
- `speck` sparingly (density ≤ 0.05) for flecks: an ore's mineral, grit in sandstone.

**Made** (planks, bricks, a crate, a bench, anything built): showing its edges is right.
- A `rows` layer for the structure: mortar lines, plank seams, a frame, legs.
- `grain` along "x" for boards laid side to side; seams every 4 rows read as planks.
- `bevel` 0.1–0.2 or a `border` one step darker frames the block, as a crafted block is framed.
- Bricks: offset the courses (vertical mortar at x = 7, then x = 3 and 11, alternating), mortar
  one step darker than the brick, not black.

## Faces that differ

- **Logs:** rings on top and bottom, bark on the sides. The rings in `rows`, light heartwood
  inside a dark bark ring one pixel wide at the edge.
- **Workstations** (bench, anvil, kiln): the top says what it's for (tools on the worktop, a
  hearth); the sides show its build (legs, a frame, a door). Keep the top's tools small: two or
  three, at the edges, so the worktop still reads as wood.
- **Grass-like** covers: the top is the cover; the side is the ground with a strip of the cover
  2–3 pixels deep along its top edge, ragged.

## Recipes to start from

Change colours from `palette.md`; keep the structure.

```ts
// stone, as the game's: nearly one colour
art.texture({ layers: [{ noise: ["#999880", "#9c9b85"], scale: 8, seed: 1 }] })

// sawn boards
art.texture({
    palette: { s: "#7a4f2a" },
    layers: [
        { grain: ["#a8743f", "#b5824a", "#c0905a", "#c99b63"], along: "x", seed: 3 },
        { rows: Array.from({ length: 16 }, (_, y) => (y % 4 === 3 ? "s".repeat(16) : ".".repeat(16))) },
    ],
})

// bark
art.texture({ layers: [{ grain: ["#532a1c", "#572d20", "#6b3a22", "#6a3e23"], along: "y", seed: 11 }] })
```

See `examples/workshop/server/art.ts` for a bench, bricks and a log end with rings.

## What the sheet catches here

- **Stripes or diagonals in the tiled patch:** the noise contrast is too high or its scale too
  small. Use fewer, closer colours and a larger scale.
- **A grid across the tiled patch** on a natural block: remove `bevel` and `border`.
- **Too dark or too cold beside the game's:** the game is warm and light. Move up its ramp.
- **The top and side look the same** on a block that should differ: give the top its own texture.
