// The base commands every server has: talking to someone, seeing who's here, and an admin's
// kick and announce. They were Luanti's builtin Lua; a mod's command shadows builtin's of
// the same name (/msg, /kick) while it runs.
//
// Permissions are named here (commands:kick, commands:announce); until roles are granted
// permissions, admins hold them all and nobody else any.

game.commands.create("msg", {
    description: "Say something to one player",
    args: { to: { type: "player" }, text: { type: "string" } },
    run({ player, args, reply }) {
        if (args.to === player) throw new Refused("That's you.")
        args.to.tell(`${player?.name ?? "The server"} to you: ${args.text}`)
        reply(`To ${args.to.name}: ${args.text}`)
    },
})

game.commands.create("players", {
    description: "Who's here",
    run({ reply }) {
        const here = game.players.list().map(p => p.name)
        reply(here.length === 0 ? "Nobody's here." : `${here.length} here: ${here.join(", ")}`)
    },
})

game.commands.create("kick", {
    description: "Disconnect a player, saying why. They can come back.",
    args: { who: { type: "player" }, reason: { type: "string", optional: true } },
    permission: "commands:kick",
    run({ player, args, reply }) {
        args.who.kick(args.reason ?? `Kicked by ${player?.name ?? "the server"}.`)
        reply(`Kicked ${args.who.name}.`)
    },
})

game.commands.create("announce", {
    description: "A line to everyone, from the server",
    args: { text: { type: "string" } },
    permission: "commands:announce",
    run({ args }) {
        game.announce(args.text)
    },
})
