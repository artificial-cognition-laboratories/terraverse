// Running a version: the game's own entry (`terraverse`, from
// scripts/release/package-linux.sh) with TERRAVERSE_HOME set, its output kept
// in logs/, watched until it exits. A clean exit is a quit; anything else is a
// crash, offered as a report.

use crate::events::{self, Game, Message};
use crate::failure::{refused, Context, Result};
use crate::home::Home;
use crate::manifest::now_ms;
use std::io::Write;
use std::path::PathBuf;
use std::process::Stdio;
use std::sync::{Arc, Mutex};

#[cfg(unix)]
const ENTRY: &str = "terraverse";
#[cfg(windows)]
const ENTRY: &str = "terraverse.exe";

pub struct Runner {
    game: Arc<Mutex<Game>>,
}

/// What a session is called: when it started, to the second, in UTC.
pub fn session_name(ms: u64) -> String {
    let secs = ms / 1000;
    let (days, rest) = (secs / 86_400, secs % 86_400);
    // days since 1970 to a civil date (Howard Hinnant's algorithm)
    let z = days as i64 + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let day = doy - (153 * mp + 2) / 5 + 1;
    let month = if mp < 10 { mp + 3 } else { mp - 9 };
    let year = yoe + era * 400 + i64::from(month <= 2);
    format!("{year:04}-{month:02}-{day:02}T{:02}-{:02}-{:02}", rest / 3600, rest / 60 % 60, rest % 60)
}

/// The log a session writes, and the version named on its first line.
pub fn log_path(home: &Home, session: &str) -> PathBuf {
    home.logs().join(format!("session-{session}.log"))
}

pub const LOG_HEADER: &str = "launcher: Terraverse ";

impl Runner {
    pub fn new() -> Runner {
        Runner { game: Arc::new(Mutex::new(Game::Idle)) }
    }

    pub fn get(&self) -> Game {
        self.game.lock().unwrap().clone()
    }

    pub fn running(&self) -> Option<String> {
        match &*self.game.lock().unwrap() {
            Game::Running { version, .. } => Some(version.clone()),
            _ => None,
        }
    }


    /// Starts the game; `exited` runs on the game's exit, once it's been told to the page.
    pub fn launch(
        &self,
        app: &tauri::AppHandle,
        home: &Home,
        id: &str,
        exited: impl FnOnce(&tauri::AppHandle) + Send + 'static,
    ) -> Result<()> {
        if let Some(version) = self.running() {
            return refused(format!("{version} is already running."));
        }
        let dir = home.version(id)?;
        let entry = dir.join(ENTRY);
        if !entry.is_file() {
            return refused(format!("{id} isn't installed properly (it has no {ENTRY}). Remove it and install it again."));
        }

        let since = now_ms();
        let session = session_name(since);
        std::fs::create_dir_all(home.logs()).context(|| format!("creating {}", home.logs().display()))?;
        let log = log_path(home, &session);
        let mut out = std::fs::File::create(&log).context(|| format!("creating {}", log.display()))?;
        writeln!(out, "{LOG_HEADER}{id}").context(|| format!("writing {}", log.display()))?;
        let err = out.try_clone().context(|| format!("opening {}", log.display()))?;

        let mut child = tokio::process::Command::new(&entry)
            .current_dir(&dir)
            .env("TERRAVERSE_HOME", home.root())
            // the game points at the launcher for versions (release/release.h: launched)
            .env("TERRAVERSE_LAUNCHER", "1")
            .stdin(Stdio::null())
            .stdout(Stdio::from(out))
            .stderr(Stdio::from(err))
            .spawn()
            .map_err(|e| crate::failure::Failure::Refused(format!("{id} couldn't start: {e}")))?;
        set(&self.game, app, Game::Running { version: id.to_owned(), since });

        let app = app.clone();
        let version = id.to_owned();
        let game = self.game.clone();
        tauri::async_runtime::spawn(async move {
            let status = child.wait().await;
            let code = match &status {
                Ok(status) => exit_code(status),
                Err(e) => {
                    eprintln!("launcher: lost the game: {e}");
                    -1
                }
            };
            set(&game, &app, if code == 0 { Game::Idle } else { Game::Crashed { version, session, code } });
            exited(&app);
        });
        Ok(())
    }

    /// The player has seen the crash and chosen not to report it.
    pub fn dismiss(&self, app: &tauri::AppHandle) {
        if matches!(*self.game.lock().unwrap(), Game::Crashed { .. }) {
            set(&self.game, app, Game::Idle);
        }
    }
}

fn set(game: &Mutex<Game>, app: &tauri::AppHandle, next: Game) {
    *game.lock().unwrap() = next.clone();
    events::send(app, Message::Game { game: next });
}

/// The exit code, or for a signal the shell's 128 + its number.
fn exit_code(status: &std::process::ExitStatus) -> i32 {
    #[cfg(unix)]
    {
        use std::os::unix::process::ExitStatusExt;
        if let Some(signal) = status.signal() {
            return 128 + signal;
        }
    }
    status.code().unwrap_or(-1)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_sessions_by_utc_time() {
        assert_eq!(session_name(0), "1970-01-01T00-00-00");
        assert_eq!(session_name(1_794_307_267_000), "2026-11-10T10-41-07");
        assert_eq!(session_name(951_782_400_000), "2000-02-29T00-00-00");
    }
}
