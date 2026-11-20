# Sprint 18 — Creative inventory, Minecraft's controls, things to experiment with

Goal: get in game and experiment. Every substance can be held and placed,
heated and chilled, and read. The inventory works as Minecraft's does, and
so do the keys.

## Landed

- **Crash fixed:** every quit crashed in `Wiki::~Wiki`. The launcher
  freed the shell before the wiki unregistered from it.
- **Keys, as Minecraft:**
  - Ctrl sprints (aux1 was E, the same key as the inventory, so running
    was impossible) and double-tap forward sprints too;
  - double-tap jump flies (in creative);
  - F3 for debug, F5 for the camera.
- **The inventory** (`ui/src/game/inventory`), split by job:
  - `carry.ts` owns the cursor and every rule:
    - click and right-click to pick up and place;
    - drag and drop, and throwing outside the panel;
    - shift-click between hotbar and storage, and 1–9 over a slot or
      catalogue item;
    - from the catalogue: a full stack, one, or shift for the first empty
      slot;
    - destroying with the bin, or by clicking the catalogue while carrying.
  - `CreativePanel` and `SurvivalPanel` render.
  - `InventoryScreen` is a fixed-size frame: icon tabs along the top
    (each drawn as an item from its category), the inventory tab at the
    bottom, the search always visible (typing searches everything), and E
    or Esc closes it.
- **A block of every substance** (`tf_matter/substances.lua`, generated
  from `core.matter.substances()`):
  - every substance's solid, plus its liquid where it has one at 1 atm
    (still, and glowing when hot);
  - each is the form its matter takes, registered after the world's own
    blocks so theirs win;
  - the catalogue's Matter tab shows each one as it is at room temperature.
- **Lab tools:**
  - the **heat gun** (left +100 K, right −100 K, setting the temperature
    outright);
  - the **probe**: a HUD card with the block, substance, state, fill,
    temperature, reaction heat, gas and whether it's awake.
- **New `core.matter` verbs:** `substances`, `touch`, `inspect`.

## Verified

- 413 engine tests pass.
- `bun check:terragen` 37/37, `bun check:lab`, `bun check:matter`: the new
  blocks and tools load on live servers.
- The UI typechecks and builds.

Not run: the client, so none of it has been seen by eye.

## Not done

- **The player model in the survival inventory:** a placeholder box for
  now.
- **Drag-to-spread** (Minecraft's dragging one stack across several slots).
- **Gas blocks:** a gas can be held in the world's air, not as a block.
