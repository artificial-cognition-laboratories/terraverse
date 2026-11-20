# Sprint 23 — Online: accounts, friends, joining

Play with friends the modern way: sign in, see who's online, join their world.
LAN comes later if it's needed; ArcLabs accounts are free, so "online" is the
default path, not the advanced one.

## Decisions

- **Identity is ArcLabs.** Terra Firma is its own product under the ArcLabs
  umbrella; the only thing it shares is the account. The backend's auth is
  treated as an identity provider that happens to live in the Axon backend
  today and can be pulled out later. The game never touches Axon tables or
  scopes.
- **ArcLabs auth replaces Luanti's.** No server passwords, no `auth.sqlite`.
  Singleplayer needs no account. Online play requires one. Players are keyed
  by ArcLabs user id per world; the in-game name is assigned once and stays.
- **No relay yet** (no new services). Connections are direct: IPv6, UPnP,
  then UDP hole punching with public STUN, coordinated through the existing
  backend. Pairs that can't connect get a plain message, not a hang. A relay
  slots in later as the last fallback.

## Steps

1. **Sign in (this sprint, built).** The backend's device flow learns which
   product asked (`device_codes.client`); a `terrafirma` approval mints a key
   with only `terrafirma:play`. `GET /api/terrafirma/me` → `{ id, username,
   name }` (no email). In the game: `src/shell/account.*` runs the flow over
   Luanti's async HTTP, keeps the key in `<user>/account.json` (0600), checks
   it at startup; bridge verbs `account.get/signIn/cancel/signOut/retry`, the
   `account` message; the account chip on the main menu (`ui/src/account`),
   every state in the gallery's Account page.
2. **Friends and presence.** Backend tables and endpoints (requests, accept,
   list; presence heartbeat: online / in a world / hosting, friends can join);
   the Friends panel.
3. **Joining.** `AUTH_MECHANISM_TOKEN`: the server's key pair (fingerprint is
   its id), a nonce in HELLO, a 60-second join JWT from the backend bound to
   that server and nonce, verified locally against the JWKS (EdDSA). The
   accounts store keyed by user id replaces `auth.sqlite`; SRP and the
   password verbs are deleted. "Friends can join" as a switch on a running
   world. Direct → IPv6 → UPnP → STUN + hole punch.
4. Build id in the handshake, CI releases (Linux, Windows), then the relay,
   then LAN.

## Outcome (step 1)

Built: backend (arclabs repo) — migration `20261024120000_device_codes_client`,
`DEVICE_CLIENTS` in `platform/auth/device.ts`, `GAME_SESSION_SCOPES`,
`/api/terrafirma/me`, the approval page names the product. Game — `Account`,
the bridge verbs, the chip and card.

Checked: engine builds, server boots, UI typechecks and builds, the backend
typechecks and its scope-vocabulary tests pass, the card's states and the chip
render in the gallery. Not yet run: a real sign-in, which needs the backend
deployed (migration first) — until then the game shows "ArcLabs answered 404".

Known: signing out forgets the key locally but doesn't revoke it (the key
can't manage keys by design); revoke from the ArcLabs site. The backend's
device `authorize` is still unauthenticated and unrate-limited (arclabs
`debt.md`) — fix before public traffic.

## Outcome (step 2: friends, presence, invites)

Built: backend (arclabs) — migration `20261024130000_terrafirma_social`
(friendships stored once per ordered pair, presence as a heartbeat read
offline after 90 s, the latest invite per pair expiring after 30 min),
`platform/terrafirma/social.ts` (`Social`, on the platform as `terrafirma`),
routes under `/api/terrafirma/`: `social` (one read: friends with presence,
requests both ways, invites), `friends` (request by exact username; a crossed
request accepts), `friends/:id/accept`, `friends/:id` (unfriend, decline,
cancel), `presence`, `invites`, `invites/:id`. All behind `terrafirma:play`.

Game — `src/shell/social.*`: polls the view every 15 s and beats presence
every 30 s (now, on any change) without blocking; the player's actions are
async bridge verbs answered with the backend's refusal. Presence is the menu,
the player's own world (`hosting`, `open` = friends can join, off on every
entry), or the Lab (`playing`, never joinable). Signing out or closing the
game goes offline at once. UI — the menu corner's Friends button (badge:
requests and invites waiting), the friends panel (add, accept, decline,
cancel, unfriend with a second press, invite, the open toggle), Friends in
the pause screen, an invite toast in menu and game. Every panel state in the
gallery's Account page.

Checked: 21 backend checks of `Social` against the local database (requests,
crossed requests, refusals, presence freshness, invites needing friendship and
a world, replacing, dismissing, unfriending clearing invites); backend
typecheck and scope tests; engine build and headless boot; UI typecheck and
build; the panel in the gallery and the corner flow against the mock.

Not wired: **Join** shows on an open host and on invites but is disabled and
says so — connecting is step 3. "Friends can join" is per session, not
remembered per world.
