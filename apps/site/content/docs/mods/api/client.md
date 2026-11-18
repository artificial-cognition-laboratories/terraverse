---
title: client
description: "The server half's channel to its own client half on each player's machine. Hear what they ask, and tell them things, as declared in mod.config.ts."
---

# client

Your server half's channel to its own client half, on each player's machine. Messages are
declared once, in [mod.config.ts](/docs/mods/structure/config#messages), and named by who
receives them.

```ts
interface ClientApi {
    // A client asked. The payload is already checked against its declaration.
    on<M extends keyof ServerMessages>(
        message: M,
        fn: (e: ServerMessages[M] & { player: Player }) => void | Promise<void>
    ): Disposer

    // Tell one player's client, or several.
    send<M extends keyof ClientMessages>(
        to: Player | readonly Player[],
        message: M,
        payload: ClientMessages[M]
    ): void

    // Tell every connected player's client.
    broadcast<M extends keyof ClientMessages>(message: M, payload: ClientMessages[M]): void
}
```

`ServerMessages` is what you declared under `messages.server`, and `ClientMessages` what you
declared under `messages.client`.

```ts
client.on("bellows", ({ player }) => {
    const pos = kilnNear(player)
    if (!pos) throw new Refused("No kiln within reach.")
    player.world.matter.heat(pos, 40_000)
})

client.send(player, "crack", { pos })
```

## Checked, not trusted

A message's *shape* is checked before your handler runs: a payload that doesn't match its
declaration is dropped and reported, and never reaches you. Its *meaning* isn't: a client can
send `bellows` while standing nowhere near a kiln. Whether the player may do what they're asking
is always yours to decide, here, where nobody's client can skip it.

## Errors

```ts
client.send(goneplayer, "crack", { pos })   // PLAYER_GONE
client.on("bellow", fn)                     // a compile error: not declared
```

A message to a player who has left throws rather than vanishing, so a handler that holds onto
players finds out.
