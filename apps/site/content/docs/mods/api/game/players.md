---
title: players
description: "game.players. Who is in the game. Joining and leaving, and each Player: who they are, where they are, what they may do, and the panels on their screen."
---

# game.players

Who's in the game.

```ts
interface Players {
    on(event: "join" | "leave", fn: (player: Player) => void): Disposer

    // Everyone connected now.
    list(): readonly Player[]

    // Someone connected now, by account. undefined if they aren't.
    get(name: AccountName): Player | undefined

    // Keep an account out of this server, connected or not. Kicks them if they're here.
    ban(name: AccountName, reason: string): Promise<void>
    unban(name: AccountName): Promise<void>
}
```

There's no "the player" on the server. It might have nobody on it, or a hundred people, so a `Player`
is always one you were handed: by `join`, by a block being used, by a command, by a message.

## Player

```ts
interface Player {
    readonly name: AccountName      // "@cody". unique and permanent: an ArcLabs account
    readonly connected: boolean     // false once they've left. then everything else throws PLAYER_GONE

    // The body they're in. Their world and position are its.
    readonly body: Body
    readonly world: World
    readonly position: Vec3

    // Move into another body: a rover, a camera drone, a seat a lab spawned for them.
    // Throws BODY_TAKEN if someone's in it.
    control(body: Body): void

    // Put their body somewhere: for spawns, arenas and admin tools. Not a way to travel.
    place(pos: Vec3, world?: World): void

    readonly roles: Roles
    // Whether any of their roles grants it, or it was granted to them directly.
    can(permission: PermissionName): boolean
    // A permission for this player alone, on top of their roles: flying, creative, for a while.
    grant(permission: PermissionName, e?: { for?: Duration }): Disposer

    // A line in their chat, from the server.
    tell(message: string): void

    // Disconnect them, with a reason they're shown. They can come back: see ban.
    kick(reason: string): void

    readonly ui: PlayerUi
    readonly inventory: Inventory   // what they carry, by mass. not designed yet
}

interface Roles {
    list(): readonly RoleName[]
    has(role: RoleName): boolean
    add(role: RoleName): Promise<void>      // kept: saved with the server
    remove(role: RoleName): Promise<void>
}
```

### After an await, check

```ts
game.players.on("join", async player => {
    await game.time.wait({ seconds: 5 })
    if (!player.connected) return
    player.tell("Still here?")
})
```

A `Player` you hold onto outlives their connection. Once they've left, `connected` is `false`
and anything else on it throws `PLAYER_GONE`, rather than quietly doing nothing.

### Roles and grants

A role is who someone is on this server, kept until it's taken away: `admin`, `builder`, a ranks
mod's `moderator`. A grant is a single permission, for this player, often for a while:

```ts
player.grant("core:fly", { for: { minutes: 5 } })
player.grant("core:creative")    // until revoked, or they leave
```

Grants end when the player leaves; roles are saved with the server.

### place is not travel

Distance is real in Terraverse, and getting between places is part of the game. `place` exists
because games need it, to start a match or return someone to spawn. A mod that offers `/home`
changes what the universe is, and the server choosing to run it should know that.

## ui

Your mod's panels and HUD pieces, on this player's screen.

```ts
interface PlayerUi {
    // Open a panel, with its props. Typed from the panel's .vue file.
    open<P extends PanelName>(panel: P, props: PanelProps<P>): Panel<P>

    // Show, update or (with null) hide a HUD piece.
    hud<H extends HudName>(piece: H, props: PanelProps<H> | null): void
}

interface Panel<P extends PanelName> {
    readonly open: boolean
    update(props: PanelProps<P>): void
    on<E extends PanelEventName<P>>(event: E, fn: (...args: PanelEventArgs<P, E>) => void): Disposer
    on(event: "close", fn: () => void): Disposer
    close(): void
}
```

```ts
const panel = player.ui.open("kilns:kiln", { heat: 1180, fuel: 6, firing: true })

const stop = game.time.every({ seconds: 0.5 }, () => panel.update(kiln(world, pos)))
panel.on("stoke", () => stoke(world, pos))
panel.on("close", stop)
```

Props and events come from the panel's `defineProps` and `defineEmits`. Events arrive from
someone else's machine, so they're [checked](/docs/mods/structure/config#checked-on-arrival)
before your handler sees them. Throwing `Refused` in one shows the player why.
