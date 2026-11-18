---
title: Client
description: Writing the client half of a Terraverse mod. One player's point of view, which draws, plays, listens and asks, but never decides.
---

# Client

The client half is one player's point of view. It runs on their machine, sent by the server
when they join, and it **presents and asks; it never decides.**

```ts
// client/main.ts
game.input.bind("kilns:bellows", { key: "B", label: "Work the bellows" }, () => {
    server.send("bellows")
})

server.on("crack", ({ pos }) => {
    player.camera.shake({ strength: 0.4, at: pos })
    game.sound.play("brick_crack.ogg", { at: pos })
})
```

## Do you need one?

Often not. Everything a mod declares on the server, its blocks, items, textures and sounds,
reaches every player's screen on its own. A mod that adds a hundred blocks and a crafting chain
can have no client half at all.

Write one when a player should *see* or *do* something a declaration can't say: a panel for
your machine, a keybind, a gauge on the HUD, a shake when a kiln cracks.

## Exactly one player

Here there is a "you": `player`, the person this is running for. It's read-only, and it's only
what the server has sent them: where they are, what they hold, the blocks around them. The rest
of the universe isn't here, and nothing here can change it.

## Presents and asks

The client half can draw panels and HUD pieces, play sounds and effects, move the camera within
what the player allows, and listen for keys. When the player wants something to *happen*, it
asks the server:

```
press B  →  server.send("bellows")  →  the server decides  →  the kiln gets hotter
                                                                  →  the panel's next props show it
```

That round trip is quick, and it's what keeps every screen honest: what a player sees is what
the universe is, never what their client hoped would happen. It's also why a modified client
can't cheat: it only ever knew what it was sent, and it can only ever ask.

## Panels

A panel is a `.vue` file in `client/`, built from the game's own components. Its **props** are
what the server sends it; its **events** are what it asks for.

```vue
<!-- client/kiln.vue -->
<script setup lang="ts">
defineProps<{ heat: number; fuel: number }>()
defineEmits<{ stoke: [] }>()
</script>

<template>
    <Panel title="Kiln">
        <Meter label="Heat" :value="heat" :max="1800" unit="K" />
        <Button :disabled="fuel === 0" @click="$emit('stoke')">Stoke</Button>
    </Panel>
</template>
```

The server opens it with those props, typed from this file, and answers its events:

```ts
// server/main.ts
const panel = player.ui.open("kilns:kiln", { heat: 1180, fuel: 6 })
panel.on("stoke", () => stoke(pos))
```

A panel's events cross the network the same way messages do, so they're checked against its
`defineEmits` as they arrive.

Panels look like the game because they're made of the game: same components, light, fonts and
keyboard handling, and they keep matching when the game changes.

## Talking to the server

Beyond panels, the two halves exchange messages, declared once in
[mod.config.ts](/docs/mods/structure/config#messages) and named by who receives them:

```ts
// mod.config.ts
messages: {
    server: { bellows: {} as {} },              // client → server
    client: { crack: {} as { pos: Vec3 } },     // server → client
}
```

Here you talk to the server with `server.send` and `server.on`; there, it talks back with
`client.send` and `client.on`. Rename a message and both halves stop compiling until they agree.
Every message is checked against the declaration as it arrives, so the server never acts on a
payload a modified client made up.

## What it can't reach

The world, other players' screens, other mods' client halves except through what they export,
the network, and the player's files. The build fails if `client/` imports from `server/`, and
`world` doesn't exist here to call. A client half can only ever be wrong about how something
looks, never about what happened.
