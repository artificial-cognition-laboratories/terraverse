# Items

An item is a hand-sized thing: a 16 × 16 icon with clear pixels round it. The inventory draws
the icon; held, the game extrudes it one pixel thick, so **the silhouette is the item**: what
matters is its outline, read at 1× in the hotbar.

## The shape of a tool

- **Diagonal:** the haft runs from bottom-left to top-right at 45°, as one-pixel steps; the
  head is at the top right. Every hand tool in the game sits this way.
- **The haft:** two pixels wide, light on its upper-left side (`w`) and dark on its lower-right
  (`W`), so it reads as round. It runs from under the head down to one pixel short of the
  bottom-left corner: 11–12 steps for a blade tool, 8–9 under a head that crosses it (hammer,
  pick). Each row moves exactly one pixel: two rows in the same columns put a kink in it.
- **The head:** the part that does the work, bold and solid. Its working edge (an axe's blade,
  a pick's points, a chisel's tip) is the lightest part.
- **Hand-sized:** the head is smaller than the haft is long — at most 6 × 4 pixels for a hand
  tool, about a third of the icon. A head as big as the haft reads as a sledge or a polearm.
- **Level heads stay level.** A head that crosses the haft (hammer, pick, mattock) is drawn
  *horizontal*, with the diagonal haft rising into its middle — not tilted to match the haft:
  a tilted block at 16 px comes out jagged and reads as an axe. Blades that continue the haft
  (chisel, knife, spear) follow the diagonal.
- **The binding** where head meets haft: 2 × 2 pixels at least, in a contrasting light colour
  (lashing `#c9b27c`, a ferrule). Less vanishes at 1×, and it's what makes a stone tool read as made.
- **A stone head isn't one grey.** One or two darker chips (`S`) on its face, and its top-left
  edge a step lighter, so it reads as knapped stone, not a flat bar.
- Then `bevel` (0.2–0.25) for the light, and `outline` last, in a near-black that's warm
  (`#14110e`), not pure black.

```ts
art.texture({
    palette: { w: "#8a5d34", W: "#5e3b1f", s: "#8f8b84", S: "#5d5a55", b: "#c9b27c" },
    layers: [
        { rows: [ /* the silhouette: see examples/workshop/server/art.ts, stoneAxe */ ] },
        { bevel: 0.2 },
        { outline: "#14110e" },
    ],
})
```

## Telling tools apart at 1×

Each tool needs a head that no other tool has:
- **axe:** a broad blade to one side of the haft, the edge curved;
- **pick:** a curved bar across the haft's top, a point at each end;
- **chisel / knife:** a narrow tip in line with the haft;
- **hammer:** a short level block on top of the haft, about 6 × 3, the haft entering under its
  middle; the striking face is its **right-hand end**, one pixel taller than the rest above
  *and* below, and a step lighter, so the business end shows at 1×;
- **saw:** a long straight blade with teeth (alternate pixels) along one edge.

If two of your items look alike at 1×, change the head, not the colours.

## Material, by colour

The head's colour says what it's made of; keep the form and change the palette:
- **stone:** greys with a warm tint (`#8f8b84`, `#5d5a55`);
- **flint:** dark blue-grey, a light edge (`#3b3a44`, `#24232b`);
- **copper / bronze:** `#c06d3e` / `#b08040` ramps; **iron:** cool greys, lighter than stone.

## What the sheet catches here

- **Too big:** the head dwarfs the haft. Shrink the head before anything else.
- **A blob at 1×:** the head is too small or merges with the haft. Make it bigger and set it apart.
- **It looks flat:** there's no light side. Add `bevel`, or put light pixels on the upper-left.
- **Ragged outline:** stray single pixels on the silhouette. Every outline pixel should join two others.
