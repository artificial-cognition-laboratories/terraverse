# Rename the gameid terrafirma → terraverse

Split from rename-to-terraverse (jobs bot, 2026-11-24). Step 2 of 4.

`--gameid terrafirma` (18+ scripts, the game's folder name, `world.mt`'s `gameid`) becomes
`terraverse`. Existing worlds' `world.mt` say `terrafirma`: accept it on load (and rewrite it), so
no one's world stops opening. Never touch `registry/worlds`.
