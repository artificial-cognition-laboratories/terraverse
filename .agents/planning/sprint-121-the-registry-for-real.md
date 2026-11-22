# Sprint 121: the registry for real

The site's registry, accounts and stars are built as if everything exists: every page reads
through one door (`useRegistry()`, `useAccount()`), answered today by `TEMP` mocks
(`apps/site/app/registry/mock.ts`, `apps/site/app/account/mock.ts`). This sprint makes them
true. A person can make an account, choose their name, sign terra and the game in, find a
mod by what it's for, read its source, and star it. Everything a page shows comes from
ArcLabs, and nothing on the site pretends any more.

It spans three places: the ArcLabs backend (`../arclabs/apps/backend`), terra
(`apps/cli`), and the site (`apps/site`).

## Where we are (read 2026-11-21)

**Accounts**
- ArcLabs accounts are Supabase. The backend accepts a Supabase session JWT as a bearer token
  (`server/utils/auth.ts`: session auth passes every scope), and CORS allows any origin with
  `Authorization` (`server/middleware/cors.ts`). So a static page can call the API straight from the
  browser with the visitor's session.
- Axon's site signs in with Google only, through `supabase-js` (`plugins/supabase.client.ts`, implicit
  flow). Email and password aren't used anywhere yet.
- The device flow exists (`/api/user/device/{authorize,poll,approve}`), with clients `axon-cli`,
  `terrafirma` and `terra-cli` (`platform/auth/device.ts`). Its `verification_uri` is always
  `${appUrl}/auth/device`, Axon's page. The site's `/device?code=…&client=…` is built to the same
  shape.
- Usernames are claimed by `PATCH /api/user/profile` (`platform/users/users.ts`). The rule is
  `^[a-z0-9][a-z0-9._-]{1,28}[a-z0-9]$`: dots and underscores allowed, may start with a digit, 3–30.
  The site's mock rule is narrower (letters, digits, hyphens, starts with a letter, 3–24), so the two disagree.
- `GET /api/terrafirma/me` is for the game: id, username and name, no email.

**The registry**
- Terraverse's registry is the shared one, product `terra`, kinds `terra-mod` and `terra-pack`. Only
  mods are published (terra publishes `terra-mod`).
- `search` takes `q` and `page` only, and returns name, description, latestVersion, stars, owner and
  updatedAt. It has no sort, category, type, official or owner filter, and no title or installs.
- `packages/@scope/name` returns the readme, owner, stars, deprecation and versions (api, the
  manifest's title and settings, dependencies, withheld). It has no installs, createdAt, license,
  dependents or files.
- A published version's manifest is `mod.config.ts`'s `{ title, settings }`. **Categories exist
  nowhere**: not in `mod.config.ts`, not in terra, not in the backend.
- The platform already has what the pages need underneath:
  - `Stars` (`platform/registry/stars.ts`): star, unstar, isStarredBy, installs totals and per
    version, activity over a range.
  - `Dependencies.dependents()` and `dependentsCount()`.
  - Install events, recorded when a tarball is fetched (`registry/npm/[...path].get.ts`).
  - `tar.ts`, which reads a version's tarball.
- `api` is a positive integer ("1"). The site's mock shows "0.1".

**The site**
- `nuxt generate`, served as static files by nginx on Cloud Run (`apps/site/build`). The registry's 176
  prerendered pages are the mock catalogue's. A static build can't show a mod published after it.

## Decisions (asked 2026-11-21)

1. **How registry pages render.** Recommended: *render on request*. The site moves from nginx to Nuxt's
   node server on Cloud Run:
   - The landing and the docs stay prerendered (`routeRules`).
   - `/registry/**` renders on request with a short cache (`swr: 60`), so a new mod appears within a
     minute and every page has real HTML for search engines.

   The alternatives are a static shell that fetches in the browser (no HTML for crawlers, and a flash
   of nothing), or a rebuild on every publish (slow, and a webhook to run).
2. **Username rules.** Recommended: the site adopts ArcLabs' rule exactly. The namespace is shared
   with Axon, so ArcLabs can't narrow it without breaking existing names. The site's `username.ts`
   mirrors `USERNAME_RE` and the reserved names; ArcLabs stays the judge of "taken".
3. **The account emails.** Confirmation and reset emails come from the one Supabase project shared
   with Axon. Recommended: neutral templates that name neither product ("Confirm your email"), with
   redirects to wherever the request started. The alternative is product-specific templates, which
   Supabase doesn't support per site.
4. **Featured mods.** Recommended: an editorial list in the site's config (`config/registry.ts`). No
   backend flag until there's an admin screen to set one.
5. **Types.** Only mods can be published. Recommended:
   - Browse's type filter shows Mods alone until packs publish.
   - Textures, shaders and worlds stay in the code behind a flag.
   - "Official" means published under the `@terraverse` org. That's a fact of the name, not a flag.

## Phases

Each phase ends working and checked. Backend first: everything after it reads from it.

### 1. ArcLabs: accounts for Terraverse

- **Device verification by client.** Each `DEVICE_CLIENTS` entry gets its own `verifyUrl`:
  - `terrafirma` and `terra-cli` → `https://terraverse.arclabs.it/device`
  - `axon-cli` keeps `${appUrl}/auth/device`

  `authorize.post.ts` builds `verification_uri(_complete)` from it.
- **Look a code up before approving it.** New route `GET /api/user/device/:code` (auth required) → `{ client, expiresAt }`, or 404.
  - The page shows what's asking from this answer, never from the URL's `client`. A crafted link
    could otherwise claim to be the game while the code really belongs to terra.
  - `approve` already fixes the client at authorize; this makes the page honest about it.
- **A public username check.** `GET /api/users/:username/available` → `{ available, reason? }`. It
  uses the same validation as `users.update`, so the name step can say "taken" as it's typed rather
  than on submit.
- **Supabase project settings.** These are configuration, not code; record them in the arclabs README.
  - Enable the email provider with confirmation on.
  - Add `https://terraverse.arclabs.it/**`, `https://www.terraverse.arclabs.it/**` and
    `http://localhost:3517/**` to the redirect allowlist.
  - Use neutral templates for confirm, reset and magic link (decision 3).
- **Checks.** Tests for the new routes:
  - a code looked up by someone else
  - an expired code
  - the client read back
  - availability for a taken name, a reserved name, a bad shape, and a free name

### 2. ArcLabs: the registry the pages need

- **Categories.**
  - The closed list is the site's `CATEGORY_LABELS` ids: games, survival, building, machines, matter,
    world, textures, library. It lives in `platform/terrafirma/categories.ts`.
  - `parseModManifest` accepts `categories: string[]`: 1–3, each from the list, refused with a reason
    otherwise.
  - It's stored in the manifest and denormalised to a `categories text[]` column on
    `registry_artifacts` with a GIN index. A migration backfills existing mods to `[]`.
- **Search, grown.** `GET /api/terrafirma/registry/search` takes:
  - `q`
  - `sort` (`relevance | stars | updated | new | trending | installs`). `trending` is installs over
    the last 7 days, from `Stars.activityForItems`.
  - `category` (repeatable; a mod must have all)
  - `type` (`mod`; `pack` reserved)
  - `official` (`@terraverse` only)
  - `owner`
  - `page`

  Each row gains `title`, `installs`, `categories`, `type`, `official` and `createdAt`. Query params are
  validated at the route, as today.
- **Facets.** `GET /api/terrafirma/registry/facets` → per-category and per-type counts, plus totals
  (listings, installs, authors). Discover and Browse need these. Cached for 60 s.
- **The package page, grown.** It adds:
  - `title` (the latest manifest's)
  - `installs` (total, and per version)
  - `createdAt`
  - `license` (package.json's, read at publish and stored on the version)
  - `categories`
  - `dependents` (names, first 50)
  - `official`
  - `starred` (with optionalAuth; false when signed out)
  - each version's `publishedAt`
- **Files.** `GET /api/terrafirma/registry/files/@scope/name/:version` → `[{ path, size, text | null }]`, read
  from the tarball with `tar.ts`.
  - Text is included up to 256 KB a file; binaries and anything larger are null.
  - A version is immutable, so the answer is cached forever (`Cache-Control: immutable`, keyed by name
    and version).
  - A private mod 404s through `access.canRead`, as everywhere.
- **Stars.**
  - `PUT` and `DELETE /api/terrafirma/registry/packages/@scope/name/star` (auth) → `{ stars, starred }`.
  - `GET /api/terrafirma/registry/starred` (auth) → the caller's starred mods as search rows.
  - All are built on `Stars`.
- **Owners.** `GET /api/terrafirma/registry/owners/:scope` → the public profile (`users.getPublic`, or
  the org) with totals (stars, installs). Their mods come from `search?owner=`.
- **Checks.** Route tests for each: filters combined, sort order, a facet count after a publish, files
  for a text and a binary file, star twice (idempotent), unstar, starred when signed out (401).

### 3. terra: categories at publish

- `mod.config.ts` takes `categories` (typed as the closed list in `packages/types`, so a typo is a red
  line in the editor).
  - terra reads it as it reads `title`, sends it in the `mod` field, and refuses before uploading when
    it's missing or wrong.
  - `terra init`'s template includes it.
- The docs: `content/docs/mods/publish.md` and the `mod.config.ts` page say what categories are and
  list them.
- **Checks.** `apps/cli/tests` for the refusals; `bun check:registry` publishes a mod with categories
  to staging and reads them back from search and facets.

### 4. The site: real accounts

- **Supabase in the browser.**
  - Add `supabase-js`, configured from runtime config (`public.supabaseUrl`, `public.supabaseKey`: the
    anon key, public by design).
  - Use the PKCE flow, so a confirmation link and Google both return to `/auth/callback`.
- **`account/` answers from ArcLabs.** `mock.ts` and the browser store's user go; the door's verbs keep their names:
  - `signIn` → `signInWithPassword`.
  - `register` → `signUp`, then a "check your email" card until the link is followed.
  - `google` → `signInWithOAuth` with `redirectTo` = the callback, carrying `next`.
  - `claim` → the availability check as you type, then `PATCH /api/user/profile`. A 409 is a `Refused`.
  - `device.lookup(code)` → `GET /api/user/device/:code`. The page says what's asking from this, not
    from the URL.
  - `device.approve` → `POST /api/user/device/approve`.
  - `stars` → the star routes, kept in the account's state after each answer. Not optimistic: the
    count shown is the server's.
- **New pages:**
  - `/auth/callback`: finishes sign-in, then goes on to `next`.
  - `/reset`: asks for an email, then sets a new password from the email's link.
  - The "check your email" card on `/register`.
- **`username.ts`** mirrors ArcLabs' rule (decision 2).
- **Checks.**
  - typecheck and build
  - against staging: sign up by email, confirm, claim a name, sign out, sign back in with Google
  - `terral login` approved on `/device`, then `terral publish` succeeds as that name

### 5. The site: the real registry

- **Rendering (decision 1).** Move to Nuxt's node server on Cloud Run:
  - `routeRules`: `/` and `/docs/**` prerendered, `/registry/**` with `swr: 60`.
  - The Dockerfile becomes `node .output/server/index.mjs`. nginx and its config go.
  - `arc deploy site` and `scripts/deploy.ts` follow.
- **`registry/` answers from ArcLabs.**
  - `mock.ts` goes. `Registry({ api })` calls the routes above.
  - Each answer is parsed at the seam into the existing types (`types.ts`); a shape it doesn't expect
    throws.
  - `front()` is the featured list (decision 4), plus facets, plus three searches.
  - `api` is shown as "API 1", not "0.1".
- **Live servers.** There's no hosting yet, so the Discover and landing server sections go behind
  `flags.servers` (off). The flag already exists.
- **Readme safety.** The readme renderer drops raw HTML (`rehype-sanitize` in `parseMarkdown`'s
  plugins), so no script, style or iframe from a stranger. Relative image paths are rewritten to
  `/api/terrafirma/registry/assets/…`. The `TEMP` in `readme.vue` goes.
- **Types (decision 5).** Browse shows Mods alone; the other types sit behind a flag.
- **Checks.**
  - typecheck and build
  - against staging: a mod published in phase 3 appears on Discover within a minute; its page shows
    readme, versions, dependents and source; a category filter finds it; starring it changes the
    count and adds it to /registry/starred
  - a 404 for a private mod
  - every `TEMP` in `apps/site/app/{registry,account}` is gone

## Done when

- A new person signs up on the site (email or Google) and claims a name.
- They run `terra login`, approve the code on the site's `/device`, and publish a mod with categories.
- Within a minute it's on Discover. Browse finds it by category, its page shows its readme, versions
  and source, and starring it is counted.
- No mock remains in the site's registry or account code.

## Not this sprint

- Managed hosting and the live server list: they stay behind `flags.servers`.
- Texture, shader and world packs, and their publishing.
- Icons and banners: the generated covers stay.
- Profile editing (bio, links, avatar): ArcLabs has it, the site doesn't show it yet.
- Reporting a mod from its page.
- The wiki.
