# Prose and markers: Terra Firma / Luanti → Terraverse

Split from rename-to-terraverse (jobs bot, 2026-11-24). Step 3 of 4, after 1 and 2.

~790 files say Terra Firma/terrafirma, ~740 Luanti (`git grep -il`). Rename the "Terra Firma:"
code markers, comments, docs, AGENTS.md, UI strings and identifiers (`tf_` prefixes are data keys:
leave `tf_*` world settings and texture names unless a migration is written). Do it by area, a
commit each, so a reviewer can read them.

Leave `Luanti` where it names the upstream: the licence, credits, "a fork of Luanti", and stock
Luanti peers in the protocol code.
