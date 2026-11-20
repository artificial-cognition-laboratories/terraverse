# Arc: things you make

An object is its shape, its material and what's been done to it. There is no axe item: anything
with a grip is a tool, and how it works comes from what it is. Blueprints say what you want, and your
processes decide whether you can make it. See "The first evening" for the experience we're aiming at.

## The first evening
Pick up stones and sticks. Knap a stone by striking it. Lash a head to a stick with fibre you cut using
that stone. Chop a tree into a real notch until it falls. Drag the log, because it won't fit on your
belt. Cold at night pushes you toward shelter and fire, which lead to the kiln, ore and cast bronze.

## Sprint 130: constructing tools
**The goal:** from empty hands, a player makes a working axe out of a stone and a stick, by hand, and a
heavy axe and a light one measurably and noticeably differ.

### 1. Small things
Objects get a **scale**, so a tool is an ordinary `piece::Object` drawn, hit and weighed at 1/8 size.
Sixteenths of a node then become 1/128 of a node, about 8 mm, reusing the shapes, mesher and strokes.
- At 1/64 (16 mm) a stone cell weighs about 10 g, so a 400 g head is only 40 cells and has no real
  edge. At 1/128 a cell is about 1.3 g, the same head is about 300 cells and a stick is 3 cells across.
- The scale is the object's data, so it can change after playtesting.
- Needs: drawing, raycast, collision and mass honouring scale; protocol bump. Block textures will look
  far too busy at this size, so small objects are drawn in their material's colour at first.

### 2. Holding and carrying
Objects can be small enough to pick up. `F` takes one into a hand if it's light enough, and nothing
is offered if it isn't. The belt is the hotbar, a few slots, with weight slowing you.
- An item on the belt holds the object's text in its metadata, so every tool stays itself.
- The figure holds it at its grip, seen by other players too. Dropped, it's a physical object again.
- Loose stones and sticks lie on generated ground. Add flint as a material.

### 3. Controls
Normal mode: left click swings, `F` interacts, holding `F` opens what the thing offers, right click
does what the held thing does (place what's carried; a heavy swing is later). Build mode (`B`) takes over
placing and carving as they are now. **Audit first:** how today's tool rail and survival digging
map onto this hasn't been checked.

### 4. Knapping
Striking a stone that's on the ground with one in hand takes a scoop from where you hit, sized by the
strike and its angle. Material says how it breaks (flint flakes, basalt crumbles, wood won't flake);
that is data on the material, the removal is native. A bad strike can split the stone in two. Flakes
are particles for now. No menu.

### 5. Measuring a thing
A pure function, `measure(shape, material, grip)`, with unit tests:
- **Mass, balance and inertia about the grip**, from filled cells.
- **Grip:** the part of it a hand fits round. No handle means held at the centre.
- **Edge:** how little material is near the point of contact, along the direction of the swing.
- **Hardness**, from its material for now. Processing comes later.

Shown when held, as plain numbers: mass, balance, edge, hardness.

### 6. Lashing
Holding `F` with a head and a stick, aimed where they should meet, offers **Lash**. It uses fibre, cut from
grass with an edged stone. The two become one object of two parts, joined by a lashing that wears.

### 7. Swinging
Left click swings at a speed set by inertia and a person's strength, so a heavy head is slower.
- A hit carries the swing's energy into a small area set by the edge, compared to the hardness on each
  side. It removes sixteenths from the target as a real notch. A stone on bare wood barely marks it.
- The tool wears too: the edge loses cells and dulls, and the lashing frays until the head flies off as
  an object of its own.

### Done means
- `TestTool`: measure's numbers, heavy swings slower, thinner cuts deeper, harder wins, wear happens.
- Lab `tools.*`, with an `expect` on each: knap, lash, swing, notch, head comes off.
- Headless boot, UI typecheck and gallery for any new readout.
- Handed over for play, because feel can't be checked by tests: can a new player get from stones to an
  axe without being told how, and can you feel two axes differ?

### Left out on purpose
Felling and tree species, dragging, cold, stamina, flakes as objects, ghosts and blueprints in survival,
saved knapping patterns, processing and hardness changes, metal, combat.

### Risks
- **Size on the wire.** Each tool's text in item metadata may be too big once a player has many. Measure in step 2.
- **Cost of many small objects.** Dropped stones shouldn't each be an awake rigid body. Settle them.
- **Physical hits and the server.** Prediction of a swing vs the server deciding the notch; start on the server.

## Where it stands
Steps 1 and 2 are built and not yet played. Still to do from them: the figure holding things (only
the first-person hand draws them, so others can't see), weight slowing you, a smaller belt, flint (stones are
`tf_world:stone` and sticks `tf_world:timber`, so the palette needn't be re-baked yet), flat colours
for small things (they use block textures, to judge by eye first). Build-mode tools don't understand scale.
A handle's text is 6.3 kB, 8.5 kB as stored on an item: fine for a few, watch it for chests.

Step 4 (knapping) is built and not yet played, with flint. The stone's flake is long and thin along the
blow, not the shallow scoop first planned: a scoop only chamfers corners and never makes an edge.
No scar colouring or strike sounds yet (the game ships no sounds), and "Edge" words appear in chat and the item's name.

## Next
Felling, dragging, cold and fire, then the kiln.
