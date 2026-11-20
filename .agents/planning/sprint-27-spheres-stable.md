# Sprint 27 — Spheres, stable

Sprint 26 made the planet: six faces in the net, the engine crossing their
edges, terra on the sphere. This sprint makes it the only kind of planet.
Every survival world is a cube-sphere; the old flat machinery goes.

## Decided (2026-10-24)

- **Default size:** `planet_half = 2048`, the 16,384 world one block to one
  block (8 × 2048 round). R ≈ 2.6 km, orbit ~160 m/s. The world size picker
  sets the planet: 2048 / 4096 / 8192 / 16384 round → planet_half 256 /
  512 / 1024 / 2048.
- **Flat worlds:** the Lab only (worlds terra doesn't generate). A terra
  world is always a planet.
- **Old saves:** a terra world without `planet_half` (made before this) is
  refused with a clear message, not loaded flat.

## Work, in order

1. **Orbit, air, gravity for the planet.** `core.orbit` on the planet's real
   radius (surface gravity 9.81, its air's scale height fitted to it).
   Orbiting is flight through the net: columns widen with height
   (`core.terra.across`), and the map's top is above low orbit. The space
   bubbles retire. `check:flight` flies up, once round (over every face),
   and down.
2. **Corner landmarks.** The 8 corners are covered by terrain (a peak), so no
   one walks into the corner point's wall.
3. **Haze over the curve.** The far view's air measured along the curved sight
   line, not flat distance.
4. **Lua (sprint 26's phase 3).** `core.planet` complete; builtin's attached
   nodes, placement facing and area searches across edges; the game's mods
   audited for `pos + offset`.
5. **Gaps.** Fences across edges; sounds and particles at the right place;
   decorations at edges; `check:sphere`'s render timing.
6. **Only spheres.** World creation makes planets; the flat path goes (the far
   view's flat-to-globe blend, terra's polar wall and space band, the
   bubbles, every TEMP from the spike); old terra saves refused.
7. **Stability.** Load measured at the default size (the loops culler, the
   server's per-block sight cone, the far view's whole-net tile choice);
   curved culling where it's slow. Two players on different faces. AGENTS.md
   and the docs say planets are cube-spheres, and engine code steps with
   `Map::next` / `getNodeSeen`, never `p + dir`.

## Done when

A new world is a planet by default; you can walk it, build across its edges,
fly to orbit, go round it and land; nothing flat is left but the Lab; every
check passes.
