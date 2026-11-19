# @terraverse/commands

The base commands every Terraverse server has, as a TypeScript mod:

| Command | Who | Does |
|---|---|---|
| `/msg <player> <text>` | everyone | says something to one player |
| `/players` | everyone | lists who's here |
| `/kick <player> [reason]` | `commands:kick` | disconnects them, saying why |
| `/announce <text>` | `commands:announce` | a line to everyone, from the server |

Until roles are granted permissions, admins (the engine's `server` privilege) hold every one.
