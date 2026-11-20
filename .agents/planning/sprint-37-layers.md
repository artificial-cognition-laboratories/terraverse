# Sprint 37 — The native renderer is the only one

## Decision

Every globe the player sees is the Planetarium. The page's WebGL globe
(`globe.ts`, `GlobeView`, `StarView`, `SystemView`) is deleted. Overlays are
layers over the playable body's own tiles; the cutaway is the body's shells
from its card. The page keeps one painter (`worldgen/fields.ts` `paint`):
it paints the layer image from the native model's fields, and the flat map
from the same painting. Moving the painter native (and the map with it) is a
later step: then there's still one.

## Built (2026-10-28), for Cody to try

- `planetarium.layer { width, height, rgba }` (base64 RGBA, the preview's
  layout: row 0 north, column 0 longitude −π), `{ inside: true }`, or
  `{ none: true }`. A layer tints the tiles (`far::Tiles::tint`): each
  tile's own colours are kept, so Terrain comes back exactly; the layer's
  colour carries the land's shade, so relief still shows. Tiles built later
  take it too; a swapped-in body takes the page's layer.
- The cutaway (`SystemLayer::cutaway`): the body's shells (core, mantle,
  crust, ocean, ice, …) as nested spheres, a window cut toward the camera,
  for any body (the one the camera's round or flying to).
- The viewer: native for the globe always; the overlay buttons set the
  layer; the star flies out to see the whole system; the flat map is the
  one page view. Laid against the free space's edges (the strip bottom
  right, the date bottom left).
- Community's thumbnails are the world's painted flat map.
- Fixed: the menu's vignette covered the whole window, so the page took
  every mouse event (the shell gives the page any pixel it draws at all):
  it's behind Home's title only. Panning works on the creator again.

Not yet: the other bodies' surfaces (spheres in their kind's colour, the
page's painted surfaces only on the flat map); the painter native; the
cutaway's cut faces (you see the shells' insides, not flat sections).
