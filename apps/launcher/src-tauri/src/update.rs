// The launcher updating itself: Tauri's updater, reading latest.json from the
// rolling `launcher` release that CI refreshes on each launcher-v* tag
// (.github/workflows/launcher.yml), signed with the key in tauri.conf.json.
// Separate from game versions, and rare.

use crate::failure::{refused, Context, Result};
use serde::Serialize;
use std::sync::Mutex;
use tauri_plugin_updater::{Update, UpdaterExt};

#[derive(Clone, Serialize)]
pub struct Available {
    version: String,
    notes: String,
}

#[derive(Default)]
pub struct Updates {
    found: Mutex<Option<Update>>,
}

impl Updates {
    /// A newer launcher, if there is one. Not reaching the server isn't the
    /// player's problem: they can still play, so it's logged and answered as none.
    pub async fn check(&self, app: &tauri::AppHandle) -> Result<Option<Available>> {
        let updater = app.updater().context(|| "starting the updater".into())?;
        let update = match updater.check().await {
            Ok(update) => update,
            Err(e) => {
                eprintln!("launcher: couldn't check for a newer launcher: {e}");
                return Ok(None);
            }
        };
        let available = update.as_ref().map(|u| Available { version: u.version.clone(), notes: u.body.clone().unwrap_or_default() });
        *self.found.lock().unwrap() = update;
        Ok(available)
    }

    /// Downloads and installs the update found, then restarts into it.
    pub async fn install(&self, app: &tauri::AppHandle, game_running: bool) -> Result<()> {
        if game_running {
            return refused("Close the game first: the launcher restarts to update.");
        }
        let Some(update) = self.found.lock().unwrap().take() else {
            return refused("There's no launcher update to install. Check again.");
        };
        if let Err(e) = update.download_and_install(|_, _| {}, || {}).await {
            return refused(format!("The launcher update didn't install ({e}). Try again later."));
        }
        app.restart()
    }
}
