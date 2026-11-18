---
title: server
description: "The client half's channel to its own server half. Ask it things, and hear what it sends this player, as declared in mod.config.ts."
---

# server

Your client half's channel to its own server half. Messages are declared once, in
[mod.config.ts](/docs/mods/structure/config#messages), and named by who receives them.

```ts
interface ServerApi {
    // Ask the server. Typed from messages.server; a message with no fields takes no payload.
    send<M extends keyof ServerMessages>(message: M, ...payload: Payload<ServerMessages[M]>): void

    // The server told this player something. Typed from messages.client.
    on<M extends keyof ClientMessages>(message: M, fn: (payload: ClientMessages[M]) => void): Disposer
}
```

```ts
server.send("bellows")

server.on("crack", ({ pos }) => {
    player.camera.shake({ strength: 0.4, at: pos })
    game.sound.play("brick_crack.ogg", { at: pos })
})
```

`send` doesn't return an answer. The server decides, and what it decided reaches this player as
what it always would: the world changing, a panel's next props, or a message back. That's what
keeps a client from ever showing something the universe didn't do.
