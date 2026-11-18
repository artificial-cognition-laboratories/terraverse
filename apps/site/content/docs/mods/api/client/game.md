---
title: game
description: "The game as this player sees it. Sounds, effects, extra cameras, other sensors and keybinds, on their machine only."
---

# game

The game as this player sees it: what they hear, what's drawn on their screen, the views a mod
adds, and their keybinds. Nothing here touches the world, or anyone else's machine.

```ts
interface GameApi {
    readonly sound: Sound
    readonly effects: Effects
    readonly cameras: Cameras    // extra views. the player's own eyes are player.camera
    readonly sensors: Sensors    // sensors on other bodies and places this player may read
    readonly input: Input
}
```

The same name as the server half's [game](/docs/mods/api/game), and the same thing: the game
being played, as each side sees it. The server sees every world and every player; this sees what
one player sees and hears.

## sound

```ts
interface Sound {
    play(sound: SoundName, e?: { at?: Vec3; volume?: number }): void
}
```

## effects

```ts
interface Effects {
    flash(e: { colour: string; seconds?: number }): void
    particles(e: { at: Vec3; texture: TextureName; count: number; spread?: number }): void
}
```

```ts
server.on("crack", ({ pos }) => {
    player.camera.shake({ strength: 0.4, at: pos })
    game.sound.play("brick_crack.ogg", { at: pos })
})
```

A shake when a kiln cracks is for the player near it; if everyone nearby should feel it, the
server tells each of them. Effects every player should see the same way, like smoke from a
chimney, don't belong here: they come from what the block is, declared once, and every client
draws them.

## cameras

Views from somewhere other than the player's eyes: a security camera feeding a monitor, a mirror on
a cart, a map from above, a spectator's view of an arena.

```ts
interface Cameras {
    // A view from a place, drawn into a panel's <View>. Not designed yet.
    create(name: OwnName, def: { at: Vec3; looking: Vec3; fov?: number }): ViewCamera
}
```

An extra camera is drawn into a panel or a HUD piece, never in place of
[player.camera](/docs/mods/api/client/player#camera). It sees only what the server has sent this
player, as their own eyes do, so a camera can't show what's hidden from them.

## sensors

Sensors that aren't on the player's own body, but that the server lets this player read: the
camera and IMU on a rover they're driving, a thermometer block in a lab, a fixed camera over an
arena for a spectator.

```ts
// The same shape as player.sensors, over every sensor this player may read.
interface Sensors {
    list(): readonly Sensor[]
    on<S extends SensorName>(sensor: S, fn: (reading: Reading<S>) => void): Disposer
}
```

Their own body's senses are [player.sensors](/docs/mods/api/client/player#sensors). Which others
they may read is the server's to grant, like everything else a client knows.

## input

```ts
interface Input {
    // Add a keybind. The key is only a default: the player's choice wins.
    // Throws KEYBIND_TAKEN if your mod already has the name.
    bind(name: OwnName, def: { key: Key; label: string }, fn: () => void): Disposer
}
```

```ts
game.input.bind("kilns:bellows", { key: "B", label: "Work the bellows" }, () => {
    server.send("bellows")
})
```

Each keybind is listed in the game's controls under your mod, and the player can rebind it. Two
mods asking for the same default key both get it, and the controls menu shows the clash for the
player to settle. Keys the game itself needs, like movement and the inventory, can't be taken.

Mouse, held keys and analogue input aren't designed yet.
