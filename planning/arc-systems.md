# Arc: systems you make

The create screen now has every setting (tagged "not wired yet"). This arc makes
them real, so a code carries a whole system and the game plays what it says.
Sprint 103 (stretched crust) moves after this arc. Each phase ends with its tags
removed and its checks passing.

## 1. Stars (sprint 103) — built
Codes are TF10: the star's class, mass, metallicity, age and wind.
- `terragen::Star` becomes class, mass and metallicity; a mass-luminosity curve
  replaces mass⁴; lifetime from mass.
- The body model steps below 0.1 Gyr; metallicity feeds composition.
- Done when: Sun-like still gives our calibrated bodies (`TestBodies`), an O star
  gives young molten worlds, and the "nearest colour" tag is gone.

## 2. The system in the code (sprint 104) — built
Codes are TF11. Each body's land settings stay for sprint 106; start body for 105.
- `TF11` stores every body's causes directly, plus names, start body and each
  body's land settings; the default system stays short. Older codes are refused.
- `SystemSpec::forWorld` reads the code. The seed and style generate the system;
  edits are saved on top.
- The planetarium draws the draft, so added bodies appear in the view.
- Measure what each extra body costs (bake, atlas, grid), then set a cap.
- Done when: a shared code rebuilds the same system on 1 and 8 cores, and added
  or removed bodies are in the game and on the map.

## 3. Home follows its card (sprint 105) — built
Codes are TF12. `WorldCode::decode` fills the home's processes from its card, so
every reader plays the same world; the climate is pushed by `Processes::warming`.
Tilt is the body's once a system is edited. Earth at 5 AU keeps 251 K: the card
lets CO₂ build up without condensing, so it's a frozen desert, not deep cold.
- `Processes` for the home come from its card, as other bodies' already do.
- Climate takes its sunlight from star and orbit; `warmth` goes. The Temperature
  slider becomes a nudge on top.
- Tilt moves onto the body. Orbit is the physics; "days in a year" is calendar
  compression.
- Start on any solid body; giants stay visit-only.
- Done when: the default world is still Earth-like, Earth moved to 5 AU freezes,
  and a game started on the Moon plays.

## 4. Land on every body (sprint 106) — built
Codes are TF13: a body's bytes may carry its own land (`SystemBody::land`). A
body's grid is kept by what decides its land (`WorldCode::keepGrid`).
- Each body's land settings reach its generation; overlays are checked on each
  body kind, and fields that mean nothing there are fixed or hidden.
- Done when: sliders change the Moon in preview and in play, and `check:far` holds
  on a non-home body.

## 5. Spin (sprint 107) — built
Codes are TF14: a body's spin travels in its flags, and a body's winds follow its
day (`Processes::day`). Protocol 90: the sky takes a body's clock.
- Free by default at any distance; a lock is chosen. A lock or resonance sets the
  card's day (`orbitHours`), so a slow turn weakens a field. Tides that hold a body
  stand its axis square. A moon can't face its star; a planet's partner waits for 108.
- Facing the star: the climate's bands are round the line to the star, not the pole
  (`Cells(axis)`); no currents, seasons or daily swing; air carries the heat round
  (`Processes::carrying`). Earth so held: ~138 °C under the star, ~−61 °C at night.
- `System::clock`: a body's time against the engine's. A free body's day scales the
  day rule (a 48-hour world's day is twice as long); a held one follows its orbit in
  the calendar's compressed time, so the fixed sky stays fixed.
- New players on a world facing its star start on the ring (`tf_planet.spawn_position`).

## 6. Double planets (sprint 108) — built
Codes are TF15: both masses set a month, so a held moon's day changed. A double is a
moon whose balance point with its planet lies outside the planet (`partnerOf`).
- Every planet swings round its balance point (`System::position`, `balance`); the
  star's orbit is the balance point's, and a double draws each half's own circle.
- A planet can hold to its partner (`Spin::Partner`); the halves flex each other.
  Touching and the rigid Roche limit, both ways, are refused (`checkPair`).
- In the sky a body over 16 pixels' radius is its own tiles, by its own clock, with
  a night side (`Sky::drawBaked`, `SystemLayer::night`); phases, not eclipses.
- Crossing is `/travel` and the planetarium's flight: no body has a flown transfer yet.

## 7. Binary stars (sprint 109) — built, without shadows
Codes are TF16: a companion is four optional bytes in the params; style Binary with
none is the seed's (`SystemSpec::companionOf`).
- Close (planets round both, orbit at least 3× the separation) and wide (round the
  first, within a third of it); touching stars and shorter-lived companions refused.
- The card's sunlight is the combined light, so the climate follows through
  `Processes::warming`. Two stars in the sky by class colour; the light level takes
  the brighter star's height (`solar::second`).
- Not built: shadows. Planets hide the stock sun, and the shadow code reads that, so
  no planet has dynamic shadows yet, from one sun or two. Sunset colours wait for the air.

## 8. Belts (after `arc-atmosphere.md` and shadows)
- Belts in the sky and planetarium as haze and visible rocks; seeds place them
  by giants and the frost line; rings use the same data.
- Standing on small asteroids waits for lumpy small bodies, outside this arc.

## Order and risk
Phases 1–3 are one chain and come first. 4–8 can be reordered after that. The
main risks are the star-locked climate (5) and lighting with two suns (7), which
both reach past terragen into the client.
