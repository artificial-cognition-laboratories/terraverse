# Rename the engine binary luanti → terraverse

Split from rename-to-terraverse (jobs bot, 2026-11-24). Step 1 of 4; do this one first.

The binary `.output/bin/luanti` becomes `.output/bin/terraverse`: the CMake target/output name,
every script (`apps/game/scripts`, `apps/game/tests`, `scripts/`), CI workflows, the launcher
(`apps/launcher`), terra (`apps/cli`, `TERRA_CLI`, `luanti --body`), release packaging and
AGENTS.md's commands. One commit, `bun check` and a headless boot after. Keep the LGPL credit to
Luanti upstream. Installed users' launchers must find the new name (check the launcher's update path).
