# Sprint 98 — Small and icy bodies

The first of the "every body plausible" list.

## The spiky moons

`Craters`: the bowl-to-complex transition (3 km / g) overflowed a fix under
about 0.09 g, went negative, and every crater became a hole or spike up to tens
of km (Aster, Nacre, Morrow: ±60 000 blocks). Now 64-bit.

## Dry bodies

A body with no water has no waterline: the grid's step at the old "shore"
(±1/160) is gone, as is detail fading at height 0. All of it has the gentle
detail low ground had.

## Ice crust

`Processes::stiff` and `flexing`, from the card (over 30% ice, no surface
ocean), never the name:
- `Rock::Ice`, as deep as anyone digs; no lava cones, no maria.
- Relief sags: craters and detail at 40%; broad relief divided by 1 + flexing
  and smoothed, so the old continents' scarps don't stand.
- Few craters: divided by 1 + 16 × tidal heat (Nacre 34 in the grid at 25k
  radius, Hale 419, Morrow 7 720).
- `World::flexed`: double ridges 200 m high along noise zero lines, four
  crossing sets, more with flexing; chaos terrain (rafts in a jumbled matrix)
  under the hardest flexing.

Also: rounding let maria miss their share (`maria.cpp`: each pool now gets what is still owed); the create
screen no longer calls generated bodies sketches.

## Not done

- The ocean under the shell (the crust is ice all the way down).
- Plumes (a live process).
- Ice has one colour; biome Snow on top, `tf_world:ice` under.
- Morrow is Callisto-like but its card says cold desert, with air.
