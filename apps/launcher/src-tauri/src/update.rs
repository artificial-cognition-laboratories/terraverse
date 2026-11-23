// The launcher updating itself: Tauri's updater, reading latest.json from the
// rolling `launcher` release that CI refreshes on each launcher-v* tag
// (.github/workflows/launcher.yml), signed with the key in tauri.conf.json.
// Separate from game versions, and rare.
//
// Quiet by design: a newer launcher is downloaded in the background as soon as
// it's found, and installed as the launcher closes, so the next start is the new
// one. Nobody is asked, and nothing restarts under them. Restarting into it now
// is offered (Settings), never needed.

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
    // downloaded, verified against the signature, waiting for the launcher to close
    ready: Mutex<Option<(Update, Vec<u8>)>>,
}

impl Updates {
    /// Looks for a newer launcher and downloads it; answers it once it's ready to
    /// apply. Not reaching the server, or the download failing, isn't the player's
    /// problem: they can still play, so it's logged and answered as none.
    pub async fn check(&self, app: &tauri::AppHandle) -> Result<Option<Available>> {
        if let Some((update, _)) = self.ready.lock().unwrap().as_ref() {
            return Ok(Some(available(update)));
        }
        let updater = app.updater().context(|| "starting the updater".into())?;
        let update = match updater.check().await {
            Ok(Some(update)) => update,
            Ok(None) => return Ok(None),
            Err(e) => {
                eprintln!("launcher: couldn't check for a newer launcher: {e}");
                return Ok(None);
            }
        };
        let bytes = match update.download(|_, _| {}, || {}).await {
            Ok(bytes) => bytes,
            Err(e) => {
                eprintln!("launcher: couldn't download launcher {}: {e}", update.version);
                return Ok(None);
            }
        };
        let answer = available(&update);
        *self.ready.lock().unwrap() = Some((update, bytes));
        Ok(Some(answer))
    }

    /// Installs what's downloaded, if anything: called as the launcher exits. A
    /// failure leaves the old launcher in place, which still works, so it's logged.
    pub fn apply(&self) {
        let Some((update, bytes)) = self.ready.lock().unwrap().take() else { return };
        if let Err(e) = update.install(bytes) {
            eprintln!("launcher: couldn't install launcher {}: {e}", update.version);
        }
    }

    /// Installs what's downloaded and restarts into it now.
    pub fn restart(&self, app: &tauri::AppHandle, game_running: bool) -> Result<()> {
        if game_running {
            return refused("Close the game first: the launcher restarts to update.");
        }
        let Some((update, bytes)) = self.ready.lock().unwrap().take() else {
            return refused("There's no launcher update ready. It's fetched in the background; check again later.");
        };
        if let Err(e) = update.install(bytes) {
            return refused(format!("The launcher update didn't install ({e}). It'll try again next time."));
        }
        app.restart()
    }
}

fn available(update: &Update) -> Available {
    Available { version: update.version.clone(), notes: update.body.clone().unwrap_or_default() }
}
