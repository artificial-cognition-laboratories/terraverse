//! The local build, as a version: in a debug launcher (`bun dev:launcher`), "dev" plays the
//! repo's run-in-place engine (`.output/bin/terraverse`, built by apps/game/scripts/build.sh),
//! so the launcher's flows can be tried against the game being worked on. A release launcher
//! never offers it.

use std::path::PathBuf;

/// The version id the page shows and asks to launch.
pub const VERSION: &str = "dev";

#[cfg(not(windows))]
const BINARY: &str = "bin/terraverse";
#[cfg(windows)]
const BINARY: &str = "bin\\terraverse.exe";

/// The local build: its folder (the engine's share directory, run in place) and its binary.
/// None in a release launcher, or before anything's been built.
pub fn build() -> Option<(PathBuf, PathBuf)> {
    if !cfg!(debug_assertions) {
        return None;
    }
    // apps/launcher/src-tauri → the repo's .output
    let output = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../.output");
    let binary = output.join(BINARY);
    binary.is_file().then_some((output, binary))
}

/// The game's page dev server (apps/game/scripts/dev.ts), when `bun dev` has it up: the local
/// build shows that page, hot-reloading, as `bun dev`'s own game does. Without it the build shows
/// its staged copy (.output/ui/dist), which is only as new as the last `bun run build`.
pub fn page() -> Option<&'static str> {
    const PAGE: &str = "http://localhost:5173";
    let address = std::net::SocketAddr::from(([127, 0, 0, 1], 5173));
    std::net::TcpStream::connect_timeout(&address, std::time::Duration::from_millis(200)).ok().map(|_| PAGE)
}
