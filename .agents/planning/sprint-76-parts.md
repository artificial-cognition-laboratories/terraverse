# Sprint 76: Parts you can see and carry

Joints became things: a sprung corner's hub rides its wheel up and down
where you can see it, and the parts a buggy needs come ready to carry
on. Arcade handling waits till there's something to drive.

## How it plays (creative)

1. Draw a chassis (Frame, sprint 75).
2. Open the inventory (I), the **Parts** tab (bottom, beside your
   inventory): **Sprung corner**, **Steering corner**, **Driven corner**,
   **Steering driven corner**, **Wheel**, and your own saved parts.
3. Click one: it's carried till you click. Onto the chassis's side it
   goes on **sprung** (medium travel, medium firmness: right-click the
   joint to change them). A Wheel onto a hub's face goes on turning.
4. Copy a corner you've made (Ctrl+C on its hub), paste it: it goes on
   as it hung, spring and all.

## Built

- **Hubs ride their wheels** (`physics::Hub`): a sprung joint's parts are
  apart (`Machine::rigid`), the hub a kinematic body that follows its
  wheel's suspension (not its turn) each step, its weight the root's
  (`World::Impl::follow`, `follows()`). With no wheel on it, it rides the
  root as if welded. It's drawn as every rigid group is.
- **A copy keeps how it hung** (`Machine::Plug`, from `extract`, at the
  end of its text): joined face to face as welded, it's joined so
  instead. The carry's hint says which.
- **Starter parts** (`workshop/starters.*`): a steel hub with a rubber
  wheel off it, turning as named, plugged on a spring; a lone wheel
  (plugged rotary). Built from the game's shapes and materials; listed
  after the player's own (theirs of a name wins), can't be forgotten.
- **The inventory's Parts tab** (`PartsPanel.vue`).
- **Fixed:** a wheel on a long soft spring rested well above where it
  was built (the spring had no preload: it only worked when the sag was
  half the travel). It droops by its sag (or half its travel) and the
  rest is preload (`droop`, `rise`).
- Protocol 82.

## Not yet

- Bearings and hinges drawn as parts (only the hub moves, the joint's
  still a mark while building).
- A spring drawn between two points (Besiege's), for odd builds.
- Icons for parts.
- Arcade handling (grip, anti-roll, air control): once there's a buggy.

## Tests

`TestMachine` 17 (`testStarters`; plugs in `testExtracted`; the hub its
own group in `testSprung`), `TestPhysics` 32 (`testHubsRide`; `testSprings`
now checks both rest where they were built). Lab check, a copy of a
played world booted, UI typecheck. Not run: the client.
