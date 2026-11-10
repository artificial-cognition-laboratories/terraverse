// What the backend pushes to the page while something's under way. Mirrors
// `Message` in src/backend/types.ts; the page listens for "launcher".

use serde::Serialize;
use serde_json::Value;
use tauri::Emitter;

#[derive(Clone, Debug, Serialize)]
#[serde(tag = "state", rename_all = "camelCase")]
pub enum Game {
    Idle,
    Running { version: String, since: u64 },
    Crashed { version: String, session: String, code: i32 },
}

#[derive(Clone, Debug, Serialize)]
#[serde(tag = "kind", rename_all = "camelCase")]
pub enum Message {
    Install { version: String, received: u64, total: u64 },
    Installed { version: String },
    InstallFailed { version: String, reason: String },
    Game { game: Game },
    /// The game's own shapes (ui/src/bridge/types.ts: Account, Social).
    Account { account: Value },
    Social { social: Value },
}

pub fn send(app: &tauri::AppHandle, message: Message) {
    // the window may be closing: nothing to tell then
    if let Err(e) = app.emit("launcher", message) {
        eprintln!("launcher: couldn't tell the page: {e}");
    }
}
