# @terraverse/time

The time where you stand, and skipping the universe forward, as a TypeScript mod:

| Command | Who | Does |
|---|---|---|
| `/time` | everyone | the hour, day, year and season where you stand, and when the sun's up |
| `/time <time>` | `time:skip` | moves the whole universe forward to your next `0600` (any 24-hour time), `dawn`, `morning`, `noon` (or `day`), `afternoon`, `dusk`, `evening`, `night` or `midnight` |
| `/time skip <by>` | `time:skip` | moves the whole universe forward by your world's hours: `6h`, `30m`, `2d` |

There's no setting the time of day. With a real sky, it's where the planet is in its turn, so a
skip moves every world on together: a night skipped is a night that passed. Times are local, so
`/time dawn` is the dawn where you stand. `dawn` and `dusk` follow the sun, so they move
with the season and the latitude; the other names are fixed hours.

Until roles are granted permissions, admins (the engine's `server` privilege) hold every one.
