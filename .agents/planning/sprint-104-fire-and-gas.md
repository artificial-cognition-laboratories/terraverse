# Sprint 104: fire and gas in the wiki's scenes

Gas and flames are pictures held to the matter, as FineWater is (`shell/plume.*`):
sixteenth cubes, no cell-sized boxes.

- Each block keeps as many puffs as its gas calls for. Steam is white; a gas with
  a real colour shows fully; clear ones barely show.
- A reaction's `flame` colour (reactions.json) says it has a flame and what colour.
  A flame is drawn where one runs. A solid fuel's fire takes its sooty colour.
- Liquid is coloured by column, carried by what runs ahead of the matter.
- A liquid in open air isn't drawn as a slab: drawFine has it.

Conduction is right, and slow: a metre of copper takes hours (checked by hand
against k and heat capacity). Scenes that need heat quickly use a burner (`power`).

Open: premixed gases burn within a tick at x1 (methane: one frame).

Endless moves (`endless` in moves.json): the blocks repeat along z, maybe rising or
falling, and keys go by position in the repeat. The picture hops back one repeat,
with the camera. A wall kick stays a loop: it turns back at its wall.
