# Simulation scales

Terra Firma simulates a world far bigger than any machine can step in full.
So every simulated thing runs at the cheapest **tier** that keeps it honest,
and a manager moves it between tiers by where players are. We already do
this without the name: the weather is a coarse global grid with local
detail (sprint 12), the world's matter is awake only around activity
(sprint 17), and the far view is the world at lower resolution from the
same rule (sprint 20). This makes it a first-class idea.

## The tiers

| Tier | When | What runs | A boiler, say |
|---|---|---|---|
| **Live** | near a player | everything: physics, heat per block, reactions per step | pressure rising tick by tick |
| **Settled** | further out, or nothing's changing | a compiled model: rates, not steps | 2 kg of steam a minute; the coal lasts 1 h 52 m |
| **Scheduled** | far away | nothing runs; the next event waits on a timer | wakes in 1 h 52 m: fire out, pressure falls |
| **Frozen** | at rest, away from players | nothing, ever, until touched | a cart parked in a field |

## Rules

1. **One model, cheaper readings of it.** A Settled model is *measured*
   from the Live one ("steady for 30 s: here are its rates"), never written
   separately, so the tiers can't drift apart.
2. **Handovers conserve.** Matter, energy and time cross a tier change
   exactly. A boiler that ran unseen for an hour burned exactly an hour of
   coal.
3. **Thresholds, not polling.** A Settled thing computes when its next
   interesting event happens (fuel out, tank full, a pressure limit) and
   schedules it. The same shape as Lua's events: Lua never polls.
4. **Hysteresis.** Promote early, demote late (Live within 48 nodes, frozen
   past 56), so nothing flickers at a boundary.
5. **Deterministic.** The same history gives the same outcome whether or
   not anyone watched.
6. **What can't compile, freezes or rides a path.** Motion has no steady
   state: a cart rolling downhill can't be summarised. Rigid bodies away
   from players are either at rest (Frozen) or on a known path (a rail, a
   road, an orbit) and advance along it. Processes (heat, reactions, fluid
   networks, power, later crops and towns) are what compile into rates.
   Orbits are on rails by design.

## Where each system stands

| System | Live | Settled | Scheduled | Frozen |
|---|---|---|---|---|
| Machines (physics, sprint 57) | Jolt, near players or held | — | — | at rest and away; out of Jolt |
| World matter (sprint 17) | awake blocks | — | — | asleep blocks |
| Weather (sprint 12) | — (coarse grid everywhere) | the grid itself | — | — |
| Heat / fluid / power networks | to come | to come | to come | — |

Sprint 57 built Live and Frozen for machines (`physics::World`'s `Tier`).
Settled and Scheduled come with the first machines worth compiling: a
powered axle, a boiler.
