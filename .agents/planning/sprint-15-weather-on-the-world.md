# Sprint 15 — The weather acting on the world

Goal: the weather leaves marks. Snow settles where it falls and melts when
it warms, and open water freezes over in the cold and thaws again. All of
it is read from `core.terra.weather`, so the storm the player sees is the
one that whitens the ground.

## Decisions

1. **Game content, in `tf_weather/world.lua`.** The engine supplies the
   weather and Lua decides what it does. One rule,
   `tf_weather.weather_on(pos, name, weather)`, says what happens to a
   node open to the sky:
   - snow settles on walkable full blocks and leaves while snow is falling
     (> 0.05 mm/h);
   - snow cover melts above 1 °C;
   - water freezes below −3 °C;
   - lake ice thaws above 1 °C.
2. **Its own blocks:** `tf_weather:snow` (a thin layer, buildable over,
   floods away) and `tf_weather:ice` (it thaws, unlike the polar wall's
   ice, which stays). Biome snow is never melted.
3. **Runs where players are:** active block modifiers on the blocks around
   them. The weather is read once per 16-block column per 10 s (one storm
   is wide).
4. **Checked headless:** the end-to-end probe applies the rule to every
   probed column and checks the result against the weather there.

## Outcome (2026-10-23)

Landed as above. Verified:
- `bun check:terragen`: **37/37 columns right** (5 froze, 1 took snow, the
  rest correctly untouched), alongside 37/37 terrain, `core.terra` and
  weather.
- `bun check:lab` passes, and 402 engine tests pass.

Not run: the client (snow settling and melting by eye, ice over lakes).

Not done, because the systems don't exist yet:
- fire going out in rain (there's no fire);
- crops reading the season's rain (there are no crops);
- the body feeling wind chill and wet clothes (there's no body temperature).

Each one only needs `core.terra.weather` once it exists.
