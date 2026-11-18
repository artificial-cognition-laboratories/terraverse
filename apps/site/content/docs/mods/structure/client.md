---
title: client/
description: The half of a mod that runs in each player's game. Sent by the server when they join, it draws panels, plays effects and asks the server to act.
icon: vscode-icons:folder-type-client
---

# client/

The half of your mod that runs in each player's own game. The server sends it to them when
they join, and it runs until they leave.

```bash
client/
├── main.ts       # runs when the player joins
├── kiln.vue      # a panel: opened by the server, closed by the player
└── hud/
    └── heat.vue  # on the HUD while the mod is loaded
```

```ts
// client/main.ts
game.input.bind("my_mod:bellows", { key: "B", label: "Work the bellows" }, () => {
    server.send("bellows")
})

server.on("crack", ({ pos }) => {
    player.camera.shake({ strength: 0.4, at: pos })
    game.sound.play("brick_crack.ogg", { at: pos })
})
```

There is one player here, `player`, the one this is running for. Everything the client
half knows comes from the server: the blocks near them, what they hold, what your server
half sent. Nothing here can change the world. It draws, plays and asks.

## When it runs

```
player joins  →  definitions, assets and client code arrive  →  client/main.ts  →  they spawn
```

Each mod's `client/main.ts` runs in dependency order before the player is in the world, so
a keybind or HUD piece is ready by the first frame. Assets are cached by content, so a
player who has joined before downloads only what changed.

When they leave, it's all torn down. The world doesn't notice: the server half is still
running.

## Panels

Each `.vue` file is a panel, built from the same kit as the game's own menus. The file's
name is the panel's name: this is `my_mod:kiln`.

```vue
<!-- client/kiln.vue -->
<script setup lang="ts">
defineProps<{
    heat: number       // kelvin
    fuel: number       // kg of charcoal left
    firing: boolean
}>()

defineEmits<{
    stoke: []
    open: []
}>()
</script>

<template>
    <Panel title="Kiln">
        <Meter label="Heat" :value="heat" :max="1800" unit="K" />
        <Meter label="Fuel" :value="fuel" :max="20" unit="kg" />
        <Row>
            <Button :disabled="fuel === 0" @click="$emit('stoke')">Stoke</Button>
            <Button :disabled="firing" @click="$emit('open')">Open</Button>
        </Row>
    </Panel>
</template>
```

A panel's **props** are what the server sends it, and its **events** are what it asks the
server to do. That is the whole contract, and the server half is typed from it:

```ts
// server/main.ts
content.blocks.create("my_mod:kiln", {
    matter: "my_mod:fireclay",
    texture: "kiln.png",
    on: {
        use({ player, world, pos }) {
            const panel = player.ui.open("my_mod:kiln", kiln(world, pos))   // props, checked

            const stop = game.time.every({ seconds: 0.5 }, () => panel.update(kiln(world, pos)))
            panel.on("stoke", () => stoke(world, pos))                      // events, checked
            panel.on("close", stop)
        },
    },
})
```

Rename a prop and `player.ui.open` stops compiling until the server sends it. There's no message
format to keep in step by hand.

`Panel`, `Meter`, `Row` and `Button` are the game's own components, available in every file
here without an import. Your panels get the same light, edges, fonts and keyboard handling
as the game's, and keep matching when the game's theme changes.

## On the HUD

A file under `hud/` is drawn over the game for as long as your mod runs. It takes props and
events the same way, set from the server with `player.ui.hud("my_mod:heat", { … })`.

## The server decides

Pressing **Stoke** asks; the server stokes; the heat rises; the panel sees it in its next
props. If the kiln was out of fuel, nothing happens and the panel shows why.

That round trip is quick, and it's what keeps every screen honest: what the client half
shows is what the world is, never what it hoped would happen.

## What it can't reach

The world, other players' screens, the network, the player's files, and any other mod's
client half except through what that mod exports. Code that touches the world can't run
here, and the build fails if `client/` imports from `server/`.
