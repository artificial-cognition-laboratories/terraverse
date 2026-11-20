# Sprint 61 — The workbench, played

Sprint 56 built the parts of the flow (a part drawn on the ground is an
object; F puts one on the bench; sockets; carrying one onto another joins
them). This sprint makes it playable end to end, toward a little car.

The flow: in build mode, draw a shape on the ground and it becomes a loose
part. F lifts one onto the bench as **the** workpiece (one at a time). While
it floats you can still walk round and draw more parts on the ground. Pick
a loose part up, bring it to the workpiece and it snaps onto a face or a
socket. F again sets the whole thing down as one physical object.

## Done

- **Making an object crashed the server** ("only what's held is moved to
  a pose"). A still grip also says "not carried", and `physics::World::carry`
  took that as letting go. Now only starting a carry ends a hold.
  TestPhysics's held test runs the workshop's sequence.
- **Pushing** has been broken since sprint 58: the client keeps each
  object held where the server has it, so the player stopped at its face
  and the server's capsule never pushed. The body now keeps what it
  presses on held objects (`World::pushes`: as hard as a person pushes once
  they walk in at 1 m/s) and sends it (`TOSERVER_OBJECT_PUSH`, protocol 72)
  about every 0.05 s. The server applies it (`Workshop::push`) up to 150 N·s
  a packet, to objects in reach that nobody holds. Anyone with interact may
  push. TestPhysics: a held block pressed for 1.6 s gets 600 N's worth along
  +x and doesn't move.
- **The footprint step can't be skipped:** a press let go before it's
  dragged keeps you outlining until the next click. Before, a quick press
  went straight to pulling, which is the likely cause of the reported "the
  second extrusion goes perpendicular first".
- **Parts on the ground while the workpiece floats:** on the bench, the
  part brush aims at the ground as well as the workpiece, whichever is
  nearer. Other brushes still only touch the workpiece.
- **Carrying didn't follow the hands** (since planet worlds). The
  player's copy of an object only moved with every map block round it
  loaded, and the sky and a planet's seams never are, so it stayed frozen
  where it was. What's carried no longer waits for the ground. TestPhysics:
  a block with no ground loaded above y 0, held and then carried, follows
  the hands (it fails without the fix).

- **Carrying while flying:** the player's physics only stepped while it
  moved them (walking), so flying, swimming or climbing, what's carried
  never moved. While something's carried it's stepped either way
  (`Body::stepObjects`).
- **The shape's ghost on a planet** was placed from the map's origin, not
  the node pointed at, so only the pointed sixteenth showed. It's the
  stroke's shape and size again, and adding starts from the sixteenth on
  the face. The one-block cursor is gone.

## Next

- Shaped world blocks (added or cut) don't let light through, so the ground
  under a thin slab is black.
- What N does (a block from your hand) inside build mode, or gone.
- Snapping: any face (now) or only marked sockets (hinges).
- The car: a box chassis and four hinged wheels, built in game, pushed.

Proved: TestPhysics 15/15. The engine's tests pass but TestAtlasRender's
two, as before. A headless server boots clean on protocol 72. Not seen on
screen.
