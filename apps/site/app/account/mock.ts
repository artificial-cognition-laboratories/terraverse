// TEMP(2026-11-21): a pretend ArcLabs while the screens are built ahead of the
// wiring (sign-in by Supabase, /api/terrafirma/me, the device approve endpoint,
// registry stars). Everything is kept in this browser; nothing leaves it.

import type { User } from "./types"

/** The account "Continue with Google" signs in as; email and password sign in as whoever typed them. */
export const MOCK_USER: Omit<User, "username"> = { id: "mock-user", email: "you@example.com", name: "You" }

/** Names already held, as ArcLabs would refuse them: the sample catalogue's owners. */
export const MOCK_TAKEN = new Set(["ash", "wren", "pell", "oren", "mira", "tove", "kit", "juno"])

/** How long a pretend request takes, so the pages show their waiting states. */
export const MOCK_LATENCY_MS = 450
