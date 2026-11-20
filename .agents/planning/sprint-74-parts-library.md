# Sprint 74: Copy, paste, keep

A good wheel is built once. Parts are copied, pasted and kept in a
library that every world shares, and there are real materials to build
them from. The goal is a car built from beams and spoked wheels in a
light metal, with a rubber tread, that drives better than one made of
dirt because it is better built.

(The deck and chest move to sprint 75, and sprung joints after that.
The engine block stays a stand-in until power is real.)

## Built

- **Copy** (Ctrl+C while building, pointing at a part): the part and
  what hangs from it, past it from the first part (`Machine::extract`;
  the first part's copy is all of it). Its joints come with it, the
  steering and driving kinds too, and its corner is the copy's. The
  clipboard is the server's, per player.
- **Paste** (Ctrl+V): another, made anew (`Workshop::paste`), held up in
  front and carried till the next click (a sticky carry: `startCarry`).
  The carry's fitting does the rest: onto a socket it's plugged with the
  socket's joint, onto a face welded, in the open put down. Creative only.
- **The parts library** (Ctrl+S, or the rail's save): the part pointed at
  kept by name, asked for on the page (`tools.naming`). It's every
  world's now (`<user>/parts/<player>/`); a world's blueprints from before
  are copied in as it opens, never moved. The rail's library is **Parts**:
  click one to carry it (it's your clipboard too), ⇣ to put it on the
  ground, × to forget it. TOSERVER_CLIPBOARD, protocol 80.
- **Materials** (`tf_world/materials.lua`): timber, aluminium, steel,
  rubber, canvas, glass, each weighed by its matter (rubber is new in the
  substance library). A material may say its friction
  (`_machine.friction`); rubber's is 1.
- **A tyre is its tread:** a wheel grips by its rim's blocks' friction
  (`treadGrip`, within 2/16 of its radius), Jolt's tyre curves scaled
  against most blocks' 0.6. A rubber rim grips two thirds more than a
  dirt one; dirt wheels drive as they did.

- **World matter saves by substance id** (`TFWM` version 2): adding
  rubber made every world refuse to load ("saved with another matter
  table"), since the awake blocks' amounts were saved by place in the
  table. Version 1 files still read (substances are only ever added at
  the table's end).

## Not built from the plan

- The creative inventory's **Parts tab with rendered icons**: the parts
  are on the rail's list, by name.
- **Starter parts** shipped with the game.
- A **softer tyre** for rubber: Jolt's wheels have no give of their own;
  that's the sprung joint's, later.
- Pasting onto a face welds it; a copied wheel's own joint kind is kept
  only inside the copy (plug it onto a socket of the kind you want).

## How it plays (creative)

1. Build a wheel: draw it, chisel the spokes, paint the rim rubber.
2. Look at it in build mode and press **Ctrl+C**. The part, with
   whatever is joined beneath it (its subtree: a wheel's hub, a beam's
   wheels), is copied. A notice says so.
3. **Ctrl+V** puts the copy in your hands, carried the way a detached
   part is (sprint 64): it is drawn where it would fit, on a socket or a
   face; Q/E spin it, F flips it, R steps the axis. Click to join it.
   Ctrl+V again gives another. Pasted into open air it becomes a new
   object.
4. **Ctrl+S** while carrying (or with a part looked at) saves it to
   the library with a name, from a small prompt.
5. The creative inventory gains a **Parts** tab: the saved parts,
   each with a rendered icon. Taking one puts it in your hands, as a
   paste does.

Ctrl is sprint, but you don't run while building, so in build mode
Ctrl+C/V/S are bindings of their own (rebindable, in Settings ▸ Controls).

## Design

- **A copy is a blueprint.** `workshop::Blueprint` (sprint 48) already
  holds machines as text. A part's copy is the machine made of that
  part and its subtree (`Machine::extract(part)`), with the joint it
  hung from kept as its **plug**, so a pasted wheel snaps on as a
  wheel does, with its kind (steers, driven) intact.
- **The clipboard is the server's**, per player (the server is the
  authority; creative pastes are free). Survival paste is refused with
  "needs the materials" until materials cost something.
- **The library moves out of the world**, to `<user>/parts/<name>.tfb`,
  so a part made in one world is there in the next. The old per-world
  blueprints are read once into it. Its names and icons go to the page
  (`parts` message); taking one is an intent (`parts.take { name }`),
  saving is `parts.save { name }`, deleting is `parts.delete { name }`.
- **Icons** are rendered the way block icons are (`shell/items`), from
  the part's mesh, cached by the text's hash.
- **Starter parts** ship the same way, as `.tfb` files in the game
  (`tf_world/parts/`): a spoked wheel, a beam, an axle. Nothing special:
  they're saved builds, read only.

## Materials

Plain building materials, each a block family with its density from the
matter library (so weight is honest without new code):

| Material | Density (kg/m³) | Friction | Look |
|---|---|---|---|
| Timber (sawn) | 600 | 0.6 | plank grain |
| Aluminium | 2700 | 0.5 | pale grey, brushed |
| Steel | 7850 | 0.5 | blue-grey, brushed |
| Rubber | 1100 | 1.0 | near black, matte |
| Canvas | 500 | 0.6 | off-white weave |
| Glass | 2500 | 0.4 | clear, tinted |

- In the build tools' material choice and the creative inventory.
- **A tyre is its rubber.** A wheel's grip is the friction of the
  blocks on its rim, not a constant (`motion` reads the rim's
  materials; rubber grips, steel slides). A wheel whose rim is rubber
  is also given a softer tyre in Jolt (more give, so less bounce).
  That is the squishy material, for now; sprung joints come later.
- The textures are flat colours with a little noise, made in the
  import script, so no new assets need a licence.

## Tests

- `TestMachine` 15 (`testExtracted`): an arm's copy has its wheel and
  their joint's kind, not the body; the first part's is all; it reads
  back as itself.
- `TestPhysics` 30 (`testTreadGrips`): on an icy tread the engine spins
  the wheels; on rubber the car goes.
- `TestLibrary` 3: forgetting; a world's blueprints adopted, the
  library's kept where a name's taken.
- UI typecheck; the palette check; a headless boot.

Not run: the client, `check:lab`, `check:wiki`.

## Not yet

- Survival costs for pasting (the materials a part is made of).
- Sharing parts between players (files can be copied by hand).
- Sprung joints, the deck and chest, hitches.
