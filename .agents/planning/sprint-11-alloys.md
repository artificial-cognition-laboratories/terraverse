# Sprint 11 — Alloys and the early craft chemistry

Goal: the library covers what the first ages need. Metals alloy (bronze,
cast iron, steel, pewter); wood burns or chars into charcoal depending on
air; sand and soda make glass; lime slakes and sets as mortar. Everything
through the existing mixture and reaction machinery.

## Decisions

1. **An alloy is a substance made by a reaction that needs its metals
   molten** (e.g. 15 Cu + Sn → bronze, requiring liquid copper and liquid
   tin). Its properties (melting point, hardness) are its own, as data. No
   new machinery: the mixture already checks states.
2. **Hardness** (Mohs) joins substances as an optional property, so tools
   and the tech tree can compare materials later.
3. **Steel from cast iron:** blowing air through molten cast iron burns
   its carbon off (25 cast iron + 39 O₂ → 11 steel + 78 CO).
4. **Wood is cellulose:** with air it burns; hot and short of air it chars
   to charcoal and steam, which is how charcoal kilns work.

---

## Outcome (2026-10-23)

Landed: bronze, pewter, cast iron, steel, zinc, wood, soda ash, glass and
slaked lime (the library is now 50 substances); nine reactions (alloying, steel
blowing, wood burning and charring, glassmaking, lime slaking, mortar
setting); `hardness` (Mohs) on metals, shown on the Matter screen; Reactor
presets: bronze casting, steel from cast iron, charcoal kiln, campfire, glass
furnace, lime mortar.

Verified: 392 engine tests pass (new: alloys need molten metals, charring vs
burning by air, glass forms only when hot, mortar sets). UI typechecks and
builds. Not run: the client, or the presets by eye in the Reactor.
