# The landing page: a brief

Three agents each build a landing page for Terraverse, in their own worktree.
The best one ships at terraverse.arclabs.it.

## This is a new task, and the vision is yours

Treat this as if no landing page existed. The current landing in `apps/site`
(the hero, the planet, the download panel's look, the feature cards, the
footer, the topnav's look, the colours and type on the page) is placeholder
scaffolding thrown together to get a site up. It is not a design, a style
guide or a starting point. **Don't reuse it, restyle it or build around it.**
Delete whatever of it you don't want.

You're the art director, designer, copywriter and front-end engineer. Do the
research, form your own view of what this game is and how it should present
itself, and invent the visual language: layout, type, colour, motion,
components, imagery, voice. Make your own components from scratch. The
in-game UI kit is worth studying as the game's own identity (see below), but
you decide what to take from it and how to evolve it for a web page. A
page that looks like a restyled template has failed the brief, however good
its content.

## What the page must do

- **Make someone want to play within ten seconds**, and understand within a
  minute what makes this different from every other voxel game.
- **Get them downloading.** Download is the primary action, with the visitor's
  OS detected. Design it however you like; the facts (platforms, URLs, the
  install command, which platforms aren't ready) live in
  `app/config/downloads.ts`, so read them from there.
- **Carry real content, the way a AAA studio's game page does.** What the game
  is; its pillars; real imagery; what's playable today; where it's heading;
  modding and open source; system requirements; FAQ; community and source
  links. The game is free and open source: say so with pride, not as an
  afterthought.

## Learn the game first

Don't start designing until you understand what this game believes.

- `AGENTS.md`: "What the game is" (the test and the seven pillars), "What
  exists", "Core mechanics", "Making and moving".
- `planning/`: every sprint's plan and arc. Start with the `arc-*` docs,
  `simulation-scales.md` and `agents-archive.md` (every sprint's "Built"
  entry); `planning/images/` has existing captures.
- Past sessions: `~/.claude-account2/projects/-home-cody-git-terraverse/*.jsonl`
  (the main repo's Claude transcripts: how the person building this thinks
  and talks about it).
- The game's own look: the in-game component kit (`apps/game/ui/src/kit`, run
  `bun components` for the gallery), `theme.css`, the fonts (Silkscreen,
  JetBrains Mono), the accent `rgb(0 148 210)` and `»`. This is the real
  identity, unlike the site's scaffolding. Study it, then develop the site's
  style from it in your own direction.

## Fixed

1. **Honest.** Claim only what exists ("What exists" in AGENTS.md, checked
   against the code if unsure). The direction (stone to space) is welcome but
   labelled as direction, never as features. Unbuilt things say so.
2. **Real imagery.** Every picture, animation or video comes from the actual
   engine, generator, simulation or game UI: no mockups, stock art or
   AI-generated images. "This planet is real" is the brand.
3. **Licensing.** Nothing resembling Minecraft characters or branding. Any
   third-party asset needs a redistributable licence and an entry in
   `assets/CREDITS.md`.
4. **It ships.** Stays in the Nuxt app (`apps/site`), prerenders with
   `bun run build`, typechecks, works at phone width with no sideways scroll,
   and stays reasonable to load (aim under ~5 MB before the user scrolls;
   lazy-load the rest). Respect `prefers-reduced-motion`.
5. **Your lane.** The landing is yours entirely: `pages/index.vue`, its own
   layout (`layouts/landing.vue`, including its header and footer), your
   components (replace `components/home/` freely), your styles and assets.
   The docs and wiki must keep working on their own layout; leave that
   layout alone.

## Tools you have

You may run the engine, the game client and every render tool to capture
material, and build new capture tools if you need them. What exists:

- A planet renderer (`app/lib/planet.js`) and its bake (`apps/site/bake/`,
  real terragen maps). Raw material only: use it, change it, or replace it
  with something better.
- `.output/bin/luanti --planetarium <code>` with a screenshot mode
  (`apps/game/src/planetarium/standalone.cpp`).
- `scripts/terragen-tool.sh` + `scripts/terragen-check/render.cpp`: a world
  code's grid as a PNG (relief, plates, rain, biomes, currents, ...).
- `bun pose-sheet`: the figure's moves as PNGs.
- The wiki's native scenes and the Lab (`check:wiki`, `check:lab`): real
  simulations of matter, flow and machines.
- The game client itself, for in-world shots.

The main checkout's build is at `/home/cody/git/terraverse/.output/` (binaries
and the staged runtime). Use it to render; don't rebuild or modify the main
checkout. If you need engine changes for a capture tool, build in your own
worktree.

## How to work

1. **Research.** Read until you can say what this game is in one sentence of
   your own.
2. **Concept.** Write `apps/site/PITCH.md` before building: the idea, the
   story the page tells top to bottom, the sections, what imagery you'll
   capture and how, and why this suits this game.
3. **Capture and build.**
4. **Review yourself.** Screenshot your page at desktop and phone width in
   headless Chromium (`--enable-gpu --ignore-gpu-blocklist` gives it WebGL),
   and look at it critically. Iterate until you'd be proud to ship it.
5. **Finish** with `PITCH.md` updated: what you built, what's real and where
   it came from, what you'd do with more time, and anything you couldn't
   verify.

Don't commit; leave your work in the worktree. Judging weighs: does it make
you want to play, does it say what this game is, is the content real and
true, is it beautiful at every width, does it load fast.
