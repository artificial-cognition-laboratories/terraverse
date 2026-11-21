find all references of `luanti` or `terrafirma` and rename to terraverse.

luanti is the orginal fork. terrafirma was a name we had before terraverse. 
## Agent notes (2026-11-20, jobs bot)

Blocked: too large for a night and in the way of uncommitted work. `git grep -i 'luanti|terrafirma'`
finds 2762 references in 764 files, and 57 files are modified in the tree right now (sprint 120).
It also needs calls that aren't renames: the binary `.output/bin/luanti` (every script, CI, the
launcher, release packages), `--gameid terrafirma` and existing worlds' `world.mt`, the protocol/peer
names, and the LGPL credit to Luanti upstream, which must stay. Plan, as separate jobs:
1. Binary name → `terraverse` (CMake target, scripts, CI, launcher, release packaging), one commit.
2. gameid `terrafirma` → `terraverse`, accepting the old id in `world.mt` for existing worlds.
3. Prose/comments ("Terra Firma:" markers, docs, AGENTS.md) → Terraverse.
4. Leave `Luanti` where it names the upstream (licence, credits, "fork of Luanti").
Run them once the tree is committed.
