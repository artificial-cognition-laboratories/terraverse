# Sprint 79: Matter foundations

Arcade chemistry: roughly true, never exact. Things boil, burn, dissolve and
explode for the reasons they would; the numbers are close enough to reason
about, not to publish.

## Before it

- The rear wheels wobble about the axle at speed (drawn off their middle).
- "Set upright" on a machine's menu: it rolls back onto its wheels.

## The sprint

1. **Data pipeline** (`scripts/matter-data`): CoolProp bakes fluid tables
   (density, heat capacity, saturation pressure) for the fluids we model;
   NASA polynomials give each substance cp(T) and enthalpy of formation. A
   reaction's heat is computed from them and checked on load (as element
   balance is). Offline only: the game reads baked tables.
2. **Catalogue to ~120 substances**: acids and bases (HCl, H₂SO₄, HNO₃,
   NaOH), metals in acid (Al, Fe, Zn → H₂), alkali metals in water (Na, K),
   fuels (H₂, CH₄, propane, ethanol, oil), glass (silica + soda ash + lime,
   with colourants), salts that dissolve and crystallise.
3. **Mixture behaviour**: properties averaged by amount; special cases as
   data (melting lowered: salt in water, soda in silica; solubility);
   colour from composition (glass by its oxides).
4. **Performance core**: blocks sleep below a threshold; reactions indexed
   by ingredient so a block checks only its shortlist (cached per set of
   ingredients); each block steps by how fast it's changing; a budget per
   tick, most active first; a benchmark (10k awake blocks < 2 ms/tick),
   checked like the Lab. Designed so a steady process can settle into rates
   (`planning/simulation-scales.md`).
5. **Blowtorch**: burns a fuel gas in air or oxygen; its flame is as hot
   as that burns (propane/air ~2000 °C). Upgrades the heat gun.
6. **A container block**: pour a cup in, drop sodium or aluminium in, watch
   it react. A Lab scenario with checks.

## Next (80)

Water and powders that flow: partial amounts per block (a cup is a
sixteenth voxel, 1/4096 m³), virtual pipes (Mei et al. 2007), powders by
angle of repose, pouring by the cup and bucket, liquid levels drawn,
Luanti's water replaced.

## Outcome

Done:
- **Before it**: wheels turned between server poses the way they were
  going, not the short way (a wheel spinning past half a turn per update was
  glided backwards and wobbled: `content_cao` keeps each group's spin
  rate). "Set upright" on a machine's menu (`TOSERVER_UPRIGHT`,
  `physics::World::upright`: rolled back the least way, lifted clear).
- **Data pipeline**: `scripts/matter-data/build.py` (CoolProp 8, Cantera
  3.2) writes `assets/matter/thermo.json`: each substance's formation
  enthalpy (NASA polynomials, else a cited literature value; elements 0)
  and, for 13 fluids, CoolProp's saturation curve (vapour pressure and
  liquid density). Reaction heats are computed from it on load; giving one
  by hand is refused. Products made in another state than their standard
  one pay its latent heat (methane's 890 kJ is 802 with its water as steam).
- **Catalogue**: 51 → 76 substances, 30 → 46 reactions: alkali metals in
  water, metals in acid, neutralising, acid from salt, brine electrolysis,
  thermite, magnesium and zinc burning, propane, ethanol, petrol,
  gunpowder; oxides that colour glass.
- **Mixtures**: a colour from what's in them (`Mixture::colour`, by mass ×
  tint, mixed in linear light); solubility as data (`dissolves`: solvent,
  most per mole, heat of solution): hydrogen chloride stays in water as
  acid instead of bubbling off, salt makes brine.
- **Performance**: a mixture checks only reactions it holds every reactant
  of (cached until what it holds changes) and isn't run at all while still
  (nothing it could run, no change of state, < 0.25 K since); no gas or
  light between two solid blocks; conductivity and heat capacity once per
  block per step. 10 648 awake blocks: 142 → 22 ms per simulated second
  (`TestSandbox::testWorldBudget`, fails over 45).
- **Blowtorch**: feeds propane and air (Shift: oxygen) into the air in
  front of the face at ~20 kW and lights it once; the flame is the reaction.
- **Vessels**: `core.matter.add` (matter into a block); a crucible (a
  `vessel` kind: never becomes what fills it, sleeps only empty); a scoop
  that fills from a substance block and pours a cup (1/4096 m³) per click,
  eight a second held. The probe lists what's in a block, with its colour.
- Lab: "Chemistry in a crucible" (sodium in water, aluminium in acid, acid
  and base), checked headless.

Not done:
- A per-tick budget taking the most active blocks first: the world still
  steps every awake block each step (22 ms per simulated second at 10 000).
- Melting lowered in mixtures (salt water's freezing point): only
  solubility is data so far. Glass still comes from the glassmaking reaction.
- A cubic metre heats slowly under a 20 kW torch (as it would); the torch
  is for crucibles and small things until matter has finer grain.
- Dissolved substances count as liquid (no separate "dissolved" state).
