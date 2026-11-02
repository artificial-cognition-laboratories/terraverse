# Sprint 97 — Polish

The list carried since sprints 89–96, taken in one pass.

## Done

- **Liquids at the body's pull.** The world's matter ran water at 9.81
  m/s² everywhere; each air now carries its body's gravity
  (`WorldMatter::Air::gravity`, `Sky::gravity`), so water on the Moon
  runs slower.
- **Coasts by their rock** (`World::hardness`, the `HARD` table by
  province, buried by sediment): hard coasts (shields, ranges' cores)
  rise as cliffs, their first metres three times as steep to 80 m (6
  blocks at least) over the sea, the lift fading where the rock softens so
  there are no steps inland; soft coasts and those with sediment keep
  beaches. Of 2 318 coast crossings on the default world, 14% rise 8
  blocks or more at the first land column, half are beach.
- **Beaches are strands**: past 3 blocks over the sea a coastal plain is
  the land's own biome (a beach had been everything under 3% elevation,
  hundreds of blocks inland on flat coasts).
- **River bends**: the meander knows its side. Inside a bend a point bar,
  a crescent as wide as the river at the bend's apex, at the water's level
  (sand); outside, a cut bank two blocks higher than a plain bank.
- **Lake edges**: a soft lake shore is a strand of sand round the water.

Golden re-pinned (1626736796); TF9 (unreleased).

## Not done

- **The biome stripes** seen on the create screen: terragen's own render
  doesn't show them, so they're in the planetarium's painting. They lie
  along meridians, likely the atlas's segment seams. Not found without
  seeing it: wants a close screenshot.
- **The far view's seam slit** (`TestFar testNoHolesFromAbove`): passes on
  the current land; the slit itself wasn't reworked.
- **Supercontinent**: the Pangaea preset (one continent, 65% land) stands
  for it; a setting for where in the supercontinent cycle the world is
  would need the code to carry it (a new version).
- **Choosing the world kind**: the home is always the system's playable
  body; picking another to live on is a feature of its own (spawn,
  rules, the far side of the create screen), for the system builder.
