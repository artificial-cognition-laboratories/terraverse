# Sprint 99 — Worlds shaped from inside

Rock lids had no volcanoes: hot spots only built chains on moving plates, so a
lid got none, and Vesper's card ("oceans freeze at 1.40") changed nothing.
All of this comes from the card (`processesOf`), never a name.

## Body model

`Story::finish`: a change still waiting out its 0.2 Gyr hold at the end of the
history happened. Vesper's volcanoes fell silent at 4.40 Gyr and weren't told,
so it read as never volcanic.

## Plumes (`Processes::plumes`)

Volcanoes lasting a billion years or more, rock over 65% (Vesper yes; Cinder's
big core and the Moon's short volcanism no; `terragen/lid.*`):
- The thin side of the crust lies 5 km low: the dichotomy.
- One rise on its edge, 6 km, ~40° wide, lifted after the plains' lava.
- Five silent giant shields (8 km / g tall at most, slopes 1 in 14).
- Three canyons split from the flank (`World::canyoned`), 7 km at most.
- Lava capped at 30% of the surface.

## Seas long gone (`Processes::ancient_water`)

Time between oceans forming and freezing or boiling away. The usual erosion,
in proportion to that time, against a level 30% of the way up the land, with
no belts rising. Valleys stand dry. Rust dust (new biome `dust`); ice caps from
about 75°.

## Eruptions (`Processes::eruptions`)

Kind Volcanic. Craters ÷ (1 + 16 × heat). Relief a quarter, smoothed; 48 thrust
blocks 5–15 km high with broken outlines; 200 attempted paterae on the plains,
half lava lakes, with wandering rims. All Province::Volcanic, no sediment.
Sulfur crust (new biome `sulfur`) with patches of bare lava.

## Not done

- Rust and sulfur are tinted sand, not real art.
- Valleys are grid scale: no small valley networks in a column.
- Dust, wind and layered caps (the atmosphere arc).
- Cinder's scarps from a shrinking core. Its check found nothing broken.
- Morrow's card still says cold desert.
