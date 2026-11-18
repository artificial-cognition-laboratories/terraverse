---
title: server.config.ts
description: How this universe is set up. Its star system and worlds, its mods' settings, and who may join. Read without running any mod.
icon: vscode-icons:file-type-typescript
---

# server.config.ts

How this universe is set up. Which mods it runs is in [package.json](/docs/server/structure/package);
this is everything else.

```ts
export default defineServer({
    system: "TF9.dGVycmE",
    worlds: { verdant: "@verdant", ember: "@ember" },

    settings: {
        "@you/kilns": { kilnHeat: 1500 },
    },

    access: {
        join: "friends",
        roles: { admin: ["@cody"] },
    },
})
```

Read **without running any mod**, which is what lets the game's server settings edit it and
the registry show what a published server is.

[Setup](/docs/server/setup) walks through each part.

Not designed yet.
