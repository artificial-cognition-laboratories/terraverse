# Sprint 17 — The world as a sandbox

Goal: the wiki's physics runs on the world's own blocks. A hole in the
ground with wood in it is a fire. Clay walls make it a kiln because clay
conducts slowly. Lava cools into basalt, and ice in the sun melts. There
are no special furnace blocks and no recipes for heat: one substrate,
awake only where something is happening.

## Decisions

1. **One physics, two places.** The rules between blocks (conduction,
   radiation, gas mixing and pressure, reactions, matter carrying its heat)
   live in `src/sandbox/physics.*`. The wiki's dense `Sandbox` and the
   world's sparse `WorldMatter` both call it.
2. **WorldMatter** (`src/sandbox/world.*`):
   - **Awake blocks** hold a Mixture.
   - **An asleep block** is what its kind is made of, at the weather's
     temperature. An asleep solid neighbour absorbs heat and wakes once
     it's 2 K off. Asleep air is the room: oxygen in, smoke out, and gas
     pushed into it that isn't air wakes it, so fuel gas burns there as a
     flame.
   - **When its matter changes,** a block becomes the form of what now
     fills it (wood → charcoal → air; clay → fired clay; ice ↔ water; lava
     → basalt).
   - **A settled block sleeps.**
   - **The world wins:** a block changed underneath (dug, built over) is
     dropped.
   - The world is reached through callbacks, so tests run it over a map of
     their own.
3. **Blocks declare their matter** (`_matter` in each node definition):
   - a substance or rock id;
   - the **share** of the volume it takes, with the rest air in its pores
     (a log is 40% cellulose, which is why wood burns);
   - its **state** (ice is water, solid);
   - whether it is the **form** that matter takes.
   A block in a state the weather can't hold wakes at its phase boundary:
   ice on a warm day sits at 0 °C.
4. **Real fire chemistry.**
   - **Wood breaks down** (C₆H₁₀O₅ → 3 C + 2 CO + CH₄ + 3 H₂O; char yield
     ~22%), and the gases burn in the air beside it as the flame.
   - **New reaction:** carbon monoxide burning.
   - **Fast reactions:** any that would finish within a millisecond
     completes within the step (a flame), instead of shrinking every step.
   - **Data fixes:** wood had a melting point at 600 K (it decomposes
     instead) and a bulk density in place of cellulose's.
5. **`core.matter`** (`src/script/lua_api/l_matter.*`):
   - `register_block`, `heat`, `blow`, `step`, `ambient`;
   - `temperature`, `capacity`, `awake`, `happened`, `count`, `save`.
   Awake blocks are saved to `<world>/matter.bin`.
6. **`tf_matter`, the game side:**
   - registers every `_matter` block, in sorted order so it's
     deterministic;
   - steps the world every 0.5 s and tells it the weather;
   - flames (particles and a light-giving flame block in burning air),
     smoke and steam, sparks off glowing solids;
   - heat hurts;
   - dug hot blocks stay hot as items (cooling over ~2 min), burn the hand
     that holds them, and keep their heat when placed;
   - new blocks: charcoal, fired clay, kindling;
   - a **fire drill** that lights only what's light enough to catch. That
     rule is thermal mass: a block's heat capacity, not a burnable list.
   - a **bellows** that pushes air out of its front.
7. **Discovery:** reactions that happen within 16 blocks of a player are
   theirs. In game, the wiki greys reactions not yet seen. The menu's wiki
   shows everything.

## Outcome (2026-10-23)

Landed as above. Verified:
- **413 engine tests pass.** New world matter tests:
  - a log fire's flame reaches ~1800 K, while a buried log only chars
    (~880 K);
  - a hot core walled in clay holds its heat better than in granite;
  - ice warmed past 0 °C becomes water, then settles and sleeps;
  - saving and loading continue identically.
  New Mixture test: the melting plateau. The wiki's sandbox tests still
  pass on the shared physics.
- **`bun check:matter` (new):** on a live server, a clay pit with kindling
  under a log, lit as the drill lights it, for 30 simulated minutes. The
  kindling burned away, and the world recorded all of these:
  - wood breaking down;
  - carbon monoxide and methane burning;
  - wood burning;
  - charcoal smouldering and burning;
  - the Boudouard reaction.
  The flame reached 884 K and `matter.bin` saved.
- **The other checks:**
  - `bun check:terragen`: 37/37 on all four checks;
  - `bun check:wiki`: 36/36 (with the new reaction);
  - `bun check:lab` passes;
  - the UI typechecks and builds.

Not run: the client. Not seen by eye:
- fire, flames, smoke and the flame light;
- the drill and the bellows;
- hot items;
- heat damage;
- greyed reactions in the wiki.

Also fixed: the weather strip covered the wiki's Close button; it now hides
while a panel is open.

**The open question this raises (for Cody): scale.** A block is a cubic
metre, so a "log" is ~600 kg of wood. A kindling bundle (15 kg) burns away
without lighting it, which is honest physics, and it means fires have to
be built up: kindling, then more, then bigger fuel. Choices:
- (a) keep it, and add mid-size fuel (sticks, split firewood) as blocks
  with small shares;
- (b) let the drill and torches act on the surface layer of a block (an
  effective thermal mass);
- (c) give big blocks a "surface" sub-cell for heat and reactions.
(a) is cheapest and most honest. (c) is the most realistic and costs
memory per awake block.

Known gaps:
- **Liquids made by heat don't flow.** Molten metal stays in its block
  (Luanti's liquid flow and the matter don't share state yet).
- **Soils are modelled as quartz sand** (marked TEMP).
- **Ores are one mineral at 60%,** not ore in its host rock.
- **No evaporation below boiling** (as in sprint 16).
