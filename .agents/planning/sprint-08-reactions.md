# Sprint 8 — Mixtures and reactions, and the Reactor

Goal: substances react. A **mixture** (amounts of substances, a
temperature, a pressure) steps through time: reactions run where their
conditions are met, at their rates, and energy balances. The Matter
screen gains a **Reactor** to watch it live.

## Decisions

1. **Reactions are data** (`matter/reactions.json`): reactants and products
   with counts, heat released per reaction, activation energy and factor
   (Arrhenius: heat-driven reactions run at factor·e^(−Ea/RT) of their
   limiting reactant), an optional energy input (electricity or light: the
   reaction runs as fast as that power pays for its heat), and
   requirements (a substance present in a state: rust needs liquid water).
   **The engine refuses any reaction that doesn't balance**, element by
   element.
2. **The mixture** (`src/matter/mixture.*`): adaptive steps (no reaction
   uses more than a tenth of its limiting reactant per step, the temperature
   moves at most 20 K), energy from reaction heat, latent heat as substances
   change state, and exchange with the surroundings (a furnace, the air). A
   small vessel heat capacity keeps a few moles from swinging wildly.
   Driven reactions take their heat from the input, not the pot.
3. **Stateless verb:** `matter.react` steps a mixture the page holds and
   returns it, with each substance's state and each running reaction's
   rate. The Reactor calls it on a timer; time speed scales the seconds
   per step.
4. **The library grows by need:** carbon monoxide, hematite, malachite,
   copper oxide, calcite, quicklime, glucose (27 substances).

## Outcome (2026-10-23)

Landed: 12 reactions — methane, hydrogen and charcoal burning;
charcoal smouldering; the Boudouard reaction; iron and copper smelting;
malachite roasting; electrolysis; rusting; calcining; photosynthesis.

Verified:
- `TestMatter`: 8 tests. Every reaction balances. Methane doesn't burn
  unlit; lit, it heats past 1300 K on its own and burns until the oxygen
  runs out, leaving the spare fuel and exactly the right products.
  Electrolysis does nothing without power and gives H₂:O₂ at exactly 2:1
  with it. Photosynthesis needs light. Smelting and the kiln do nothing
  at room temperature and run in a furnace. A nail rusts only when wet.
  The whole engine suite passes (385 tests).
- Every Reactor preset run through the real mixture code (standalone):
  gas flame (peaks 1528 K, burns out its oxygen), charcoal fire (starved:
  all carbon monoxide), iron furnace (2 iron from 1 ore), copper smelt
  (roast then smelt: 2 copper), lime kiln, electrolysis (2:1),
  hydrogen and oxygen (flash to 1506 K, pure water), rusting nail (a
  fifth rusted in a day), leaf (sugar and oxygen).
- UI typechecks and builds.

Not run: the client (the Reactor against the engine).

Known: rates are lumped (first order in the limiting reactant), and
vessels have fixed pressure, so gases don't build pressure yet (rooms
will). No solutions or dissolving yet, and no alloys (next: a mixing rule
for melting point and hardness). Activation energies and factors are
tuned for game timescales, not measured.
