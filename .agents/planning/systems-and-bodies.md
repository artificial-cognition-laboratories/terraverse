# Systems and bodies: direction

Where the world model is going beyond Earth. Earth is the first instance of
it, not a special case. Directional: built in sprints, each one leaving the
game playable.

## A world is a history

A body is a short history, not a list of settings. A few **causes** are run
through cause and effect; everything else **emerges**. The player sets
causes, never outcomes (no "surface temperature" or "pressure" sliders).

**Causes:**
- the star: mass (so luminosity and spectrum), age, activity;
- the orbit: distance, eccentricity, inclination (for a moon, around its
  parent);
- the body: mass, composition (metal / rock / ice shares), its volatile
  endowment (the water and gases it formed with), rotation, tilt;
- its moons, and what it orbits.

**What emerges, in order:**

| Step | Gives | Plays as |
|---|---|---|
| Interior heat: radioactive + leftover + tidal, falling with age | volcanism; tectonics (plates) or a frozen lid | where ore forms; geothermal power |
| Molten core convection (metal, heat, spin) | magnetic field | radiation shielding, auroras, compasses, keeping the air |
| Outgassing − escape (gravity, temperature, stellar wind unshielded by a field) | atmosphere: pressure, composition | breathing, fire, flight, rooms |
| Starlight + greenhouse | surface temperature; water as ocean, ice, steam | climate, biomes |
| Weathering (water + tectonics) | long-term stability | Earth-like vs runaway hothouse |
| Life (optional) | free oxygen, which only life makes | breathable air is rare |
| Impacts vs resurfacing | craters, lava plains, ice shells | terrain kinds, exposed ores |

Deterministic and pure (fixed point), like terragen: dozens of numbers
through equations, cheap enough to preview live. **One implementation**, in
terragen, served to the menu, never re-derived in the page.

## Forces worth modelling

- **Tides:** tidal heating (volcanic moons, oceans under ice), tidal locking
  (day side, night side, a livable twilight ring), ocean tides from a moon
  (tidal flats, tide-gated resources, tidal power), the Roche limit (rings).
- **Magnetic fields:** surface radiation without one, atmosphere stripped
  over time without one, radiation belts that make some orbits deadly, a
  working compass with a wandering pole, auroras.
- **Atmosphere composition** feeds the rooms primitive: breathability
  (oxygen partial pressure, CO₂ toxicity), combustion needs oxygen (no fire
  on Mars without an oxidiser), pressure decides flight (Titan: wings
  work), jets need oxygen, dust storms, acid clouds, methane rain.

## Worlds that should emerge

Temperate (home); cold desert (thin CO₂, buried ice, dust storms, no field);
hothouse (crushing surface, but a breathable-pressure band high up where
floating cities make sense); volcanic moon (tidal heat, sulfur, lava);
ice-shell ocean moon (drill through ice, vents beneath); hydrocarbon world
(methane lakes, fuel everywhere, oxygen is what you bring); airless moon
(regolith, vacuum, huge day-night swings, ice in polar craters that never
see sunlight, helium-3); asteroids (metal ones are shattered cores rich in
platinum-group metals; carbon-rich ones hold water); gas giant (no surface;
skim hydrogen and helium-3; radiation; moons); comets; tidally locked
"eyeball" planets.

## Why go to space (each enforced by the mechanics)

1. **Resources Earth lacks:** platinum-group metals (on Earth they sank to
   the core), helium-3, polar ice for propellant.
2. **Manufacturing that needs space:** microgravity (perfect crystals,
   alloys that won't mix under gravity, flawless fibre) and vacuum. Endgame
   electronics need an orbital fab: an environment gate.
3. **Energy:** solar with no night, clouds or air; beamed down.
4. **Gravity wells as logistics:** launch from the moon, build ships there,
   refuel at ice depots. Delta-v becomes a supply chain.
5. **Orbit pays back on the ground:** satellites unlock maps, positioning
   and forecasts; spectroscopy from orbit finds ore bodies.
6. **Science:** samples from other bodies unlock knowledge.
7. **Heat in vacuum only radiates:** every station needs radiators sized to
   its machines.

Example of mechanics composing with no special code: on the moon, mine
crater ice (geology), melt it (heat), electrolyse it with solar or reactor
power (reactions + electric network) into hydrogen and oxygen at 2:1;
oxygen fills the habitat's sealed rooms (rooms), the rest becomes
propellant; the engine burns them (reactions); waste heat goes out through
radiators (heat).

## How it fits the engine

- **A system code** extends the world code: the star and seed generate a
  system from a simple formation model (rocky worlds inside the frost line,
  giants and icy moons beyond, a belt where a planet failed to form).
- **The body model** is a pure terragen function: causes → derived card +
  history. Earth runs on it.
- **Each landable body is a world layer** with a terragen kind; gas giants
  are orbit-only.
- **Geology (phase 5) works per body**, from that body's history.
- **Scale honestly:** our worlds are small; relative units (Earth = 1) keep
  the relationships true.

## The modelling boundary

Model **conserved quantities in reservoirs and the rules that move them**,
never the mechanism underneath. The same pattern at every scale:

- **Substances, not atoms.** A curated table of ~30–50 species (water, CO₂,
  N₂, O₂, methane, ammonia, SO₂, iron, silicate rock, sulfur, sulfuric
  acid, helium-3, …) with their properties: molar mass, density, phase
  curve, greenhouse strength, heat capacity. Elements appear only as tags,
  for conservation and ore yields. One table serves planets and furnaces
  (the matter and reactions primitive).
- **Reservoirs and fluxes, not space.** A body is a few boxes: core,
  mantle, crust, ocean, ice, air, with space as a sink. Its history is
  fluxes between them: outgassing, weathering, subduction, escape,
  freezing. Geochemists call this a box model.
- **Regime switches, not mechanics.** Mantle convection as a regime (moving
  plates, stagnant lid, heat-pipe volcanism); the core's dynamo on or off.
  Chosen by rules, calibrated on known worlds.
- **Space and fine time only where you stand:** box models per body over
  billions of years; blocks, rooms and fluids around the player over
  seconds; terrain generated from the boxes' outcomes.

**The test for adding something:** model it only if it's a conserved
quantity players can move, or a regime switch that changes what a world
is. What follows from that:
- rings: tidal migration as one flux, the Roche limit as a threshold;
- the field: from the core box;
- terraforming: the box model runs forward from today with fluxes players
  make (imported volatiles, a magnetic shield, orbital mirrors).

## Order

1. **Done (sprint 6).** Body model v1 in terragen + `system.preview` + the create screen's
   control surface (causes on the left; derived card, globe and a history
   strip on the right; System owns every body, World keeps Earth's land).
2. Geology phase 5 on the body's history.
3. The moon as the first second world (airless kind).
4. Heat, rooms and reactions in the Lab; fire failing on the moon is the
   first cross-world proof.

## Review of the first System tab sketch (2026-10-22)

Keep: inventories over outcomes, honest preview notices, adding and
removing bodies. Fix before building on it: two body models (`bodies.ts`
and `systemDraft.ts`) → one; family chosen by id → data; activity, core
convection, atmosphere and family are outcomes, not controls; derived
values computed in the component → terragen; Earth's tilt stored twice.
