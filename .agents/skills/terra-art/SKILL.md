---
name: terra-art
description: Make Terraverse blocks and items with art drawn from code (the `art` global and content.blocks / content.items in a TypeScript mod), and get them looking right by rendering and reading a sheet. Use when asked to make, draw, texture or restyle a block, an item, a tool or a set of them for Terraverse, or to make art for a mod. Never hand-writes or generates PNG files.
---

# Terraverse art

Blocks and items are declared in a mod's TypeScript, and their pictures are drawn there too:
a palette, letter grids and a few procedural layers, painted by `art.texture`. No image file is
ever made by hand or by a model. You look at what you made by rendering it to a sheet and
reading the image, and you keep going until the sheet passes the checklist below.

## The loop

1. **A mod to work in.** Use the one you're asked to, or make one: `terra mod init <name>`
   (`terral` from a Terraverse checkout). A bare name makes it in the player's profile, in
   `mods/` under their Terraverse home (`~/.terraverse-dev` from source), where the game finds it;
   never make mods in a temporary folder. It comes with `server/art.ts` for the pictures and
   `server/main.ts` for the declarations. `terra mod init` prints where it made it.
   **The reference:** `examples/workshop/` in this skill's folder is a whole mod drawn to this
   standard: four blocks and three tools. Read its `server/art.ts` and copy its shapes. You
   never render it on its own: `--beside` (step 4) puts it on your sheet.
2. **Write** the textures, then the blocks and items that use them.
3. **`terra check`** — typechecks against the mod API, then loads the mods dry, as the game
   would. A bad grid, colour or name is refused with where (`rows[3][0] is "x", which isn't in
   the palette`). Fix until it passes; it takes a second or two.
4. **`terra art render --with <game blocks>`** — writes `.terra/art/sheet.png`; read it.
   `--with` puts the game's own blocks first on the sheet (blue names) to hold yours against:
   always pass the nearest ones (`--with log,stone_bricks` for a wooden or stone build).
   `--beside <this skill's folder>/examples/workshop` puts the reference mod's blocks and tools
   first too, marked: always pass it for items (the game has no tools of its own yet). It is
   prepared where it is, into its own `.terra/`, which git ignores: that's expected.
   `--match <text>` narrows the sheet to what you're working on.
5. **Judge the sheet** against the checklist; change; go to 3. Two or three rounds is usual.
6. **Show the person.** In the game's menu, Community → Mods → **Yours** lists the mods in their
   profile; opening one shows its Contents: every block and item, turned in 3D and flat in 2D,
   as the game draws them. After you save, they press Reload. Tell them the mod's name.
7. **Show it in the world** when it passes: `terra dev`, run in the mod's own folder, starts the
   game with that mod loaded (a world opened any other way doesn't have it). G → Library: it's
   listed under the mod's name, turns in 3D and shows its pictures flat. Say what you made and
   what's left rough.

`--json` on check, render and list gives the same as data.

## What a sheet shows, and the checklist

A block: its cube as the inventory draws it, its top, side and bottom flat, and its top and
side **tiled 3 × 3** as the world repeats them. An item: large, then at 1× and 2× as the hotbar
draws it.

- [ ] **It matches the game.** Next to the `--with` blocks it looks like the same game: the
      same warmth, saturation and *low contrast*. The game's art is soft: its stone is almost
      one colour. Take colours from `references/palette.md`, not from your head.
- [ ] **It tiles.** In the tiled patches no seam, stripe, diagonal or blob lines up across
      blocks — unless the block is meant to show its edges (planks, bricks, a crate).
- [ ] **It's lit from the top-left.** Edges facing up and left lighter, facing down and right
      darker; never the other way.
- [ ] **It reads small.** An item is recognisable at 1×; a block's top is distinct from its side
      where it should be (a log's rings, a bench's worktop).
- [ ] **It's what it's made of.** Wood looks like wood of a kind the game has, stone like its
      stone. A block's `matter` is what it physically is; its picture agrees.

## The API, at a glance

```ts
const t = art.texture({
    size: 16,                          // or 32; 16 unless asked
    palette: { o: "#c0905a", d: "#5a3a20" },  // one character each; "." and " " are clear
    layers: [                          // painted in order, each over the last
        { fill: "#3a2515" },
        { noise: ["#a2a189", "#a9a790"], scale: 8, seed: 9 },   // blotches, darkest first
        { grain: ["#a8743f", "#c0905a"], along: "x", seed: 3 }, // streaks ("y": top to bottom)
        { speck: "#5f5c56", density: 0.05 },                   // single pixels
        { rows: [/* 16 strings of 16 characters */] },        // palette letters
        { border: "d", width: 1 },
        { bevel: 0.2 },                // light from the top-left; works on shapes too
        { outline: "#14110e" },        // a line round what's drawn: items
    ],
    rows: [/* optional: one more rows layer, on top */],
})

content.blocks.create("mymod:bench", {
    matter: "wood", share: 0.5, description: "Workbench",
    texture: t,                        // or { top, bottom?, sides }
})
content.items.create("mymod:axe", { icon: t, description: "Stone axe" })
```

The full types are in the mod's `.terra/types/server.d.ts` (written by `terra check`): read
them when unsure. Colours are `#rgb`, `#rrggbb` or `#rrggbbaa`, or a palette letter. Seeds make
noise repeatable; change one to get a different blotch without changing the look.

## Blocks or items

- Making a **block**: read `references/blocks.md` (faces, tiling, material recipes).
- Making an **item**: read `references/items.md` (silhouettes, tools, reading at 1×).
- Either way, take colours from `references/palette.md`.

## Rules

- Never write or generate a PNG, and never put image files in `assets/` for this: everything is
  drawn by `art.texture` from the mod's source.
- One light, top-left. Hard pixels: no anti-aliasing by hand, no gradients across a 16 × 16 face.
- Names are `prefix:lower_case` under the mod's own prefix; `description` is what players read.
- Items do nothing yet but exist, show and stack: don't promise behaviour (digging, cutting).
- Don't stop at "it typechecks". Done is a sheet that passes the checklist, read by you.
