# Sprint 24 — Invite and join, without setup

Goal: a friend clicks Join and lands in your world. No ports, no addresses,
no passwords. (Milestone A, same network, falls out of the same work;
milestone B, across the internet, is the point.)

## How it works

**Hosting.** Signed in, a world of your own runs as a real server
(`GM_HOST_AND_JOIN`) on any free port, with you named by your ArcLabs
username (a world played alone has its `singleplayer` carried over the first
time: `online/players.cpp`). `online::Host` never holds the world up (it
used to: 5 s of UPnP and STUN before the server started): a worker resolves
the STUN servers and asks the router to open the port (UPnP, miniupnpc,
optional at build); STUN requests go out from the server's own socket, and
their answers come back through the connection's stray handler — datagrams
that aren't the game's (`IConnection::setStrayHandler`, network/mtp). The
endpoint is known ~60 ms after the server starts; repeated every 20 s the
requests keep the router's mapping alive and notice if it changes. Presence carries the endpoint
`{ public, lan, nat }` — never shown to friends, only handed to a joiner with
a ticket. The world pauses only while no friend is in it.

**Joining.** Join (menu only) runs `online::prepareJoin` on the bridge's
worker: a fresh socket, STUN-mapped; `POST /api/terrafirma/join` with its
endpoint. ArcLabs checks friendship and open-or-invited, and answers with a
single-use ticket and the host's endpoint. The joiner connects to the host's
LAN address when both share a public address, else its public one — from that
same socket (`Client::connect(..., UDPSocket&&)`).

**Hole punching.** The host long-polls `GET /api/terrafirma/knocks?wait=20`;
a join appears within a second with the joiner's endpoint, and the host sends
small packets toward it from the server's socket for 8 s, while the joiner's
client keeps retrying its handshake. Each side's router now expects the
other. Between joins the host's STUN rounds keep its mapping alive.

**Admission.** Protocol 58: `AUTH_MECHANISM_TICKET` + `TOSERVER_TICKET`
`{ ticket, build }`. A hosted server offers only that. `online::Gate` (on the
server thread, asynchronous) admits its owner's own client by a secret that
never leaves the process, and anyone else by asking
`POST /api/terrafirma/tickets/verify` — the ticket must be for this host,
unused, in time, and belong to the name the client claims; ArcLabs uses it
up. Builds must match (`g_version_hash`), or the joiner is told both.

## What fails, and how it says so

- Both sides behind symmetric NATs (and no UPnP): refused before connecting —
  "your games can't reach each other directly".
- One side symmetric, or a router that drops the punches: the connection times
  out — "Couldn't reach X's world. One of your networks may not allow direct
  connections between games." The relay (not built) is the answer for these.
- Not open and not invited, not hosting, not friends: ArcLabs' reason.
- A username that can't be a player name (over 20 characters, other symbols):
  refused at Play/Join.

## Checked

- Backend: 14 checks of join/knocks/verify against the local database
  (friendship, hosting, endpoint, closed vs invited vs open, knocks once,
  tickets single-use and host-bound, endpoints never in the friends view);
  typecheck.
- Engine: `TestOnline` (STUN request bytes; a real Google STUN answer decoded;
  answers for other transactions, truncated packets and game packets
  ignored; endpoint JSON; routing — same network, across the internet,
  unknown). A one-off live run of `Host` on a bare socket: constructed in 0 ms,
  endpoint known at 63 ms (`Host` STUN via `stray`). A one-off live probe: STUN answered in 41 ms with this machine's
  public address and port (port-preserving cone NAT). Build; headless boot.
- UI: typecheck, build; Join in the gallery (and refused against the mock).

Not run here: two games joining (needs two clients, the user runs it), and a
join across two real networks (needs a second network — a phone hotspot).

## Not yet

- The relay, for pairs no punch gets through.
- LAN discovery; IPv6 candidates; trying more than one address.
- Windows: LAN addresses aren't read (it says so); the join still works by
  public address.
- Revoking a ticket's player mid-game (unfriend kicks nobody yet).
