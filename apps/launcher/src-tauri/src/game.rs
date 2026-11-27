// Running a version: the game's own entry (`terraverse`, from
// apps/game/scripts/release/package-linux.sh, or `bin\terraverse.exe` from package-windows.sh) with TERRAVERSE_HOME set, its output kept
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
const ENTRY: &str = "bin\\terraverse.exe";

/// The game the page shows (`game`), and every profile a game is running as: one game per profile
/// (each holds its profile's worlds and sign-in), and any more beside the shown one are extras,
/// started by Play as.
pub struct Runner {
    game: Arc<Mutex<Game>>,
    profiles: Arc<Mutex<Vec<String>>>,
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

/// What's launched: a version, for a profile, maybe straight into a friend's world.
pub struct Launch<'a> {
    pub home: &'a Home,
    pub id: &'a str,
    /// the ArcLabs API the launcher's signed in to: the game signs in there too
    pub api: &'a str,
    /// None: the active profile
    pub profile: Option<&'a str>,
    /// "<account id>:<handle>": joined once the game's signed in
    pub join: Option<&'a str>,
}

impl Runner {
    pub fn new() -> Runner {
        Runner { game: Arc::new(Mutex::new(Game::Idle)), profiles: Arc::new(Mutex::new(Vec::new())) }
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


    /// Starts the game, as `profile` (None: the active one); `exited` runs on the game's exit, once
    /// it's been told to the page. With a game running, another profile's starts beside it: one
    /// game per profile, never two.
    pub fn launch(&self, app: &tauri::AppHandle, run: Launch<'_>, exited: impl FnOnce(&tauri::AppHandle) + Send + 'static) -> Result<()> {
        let Launch { home, id, api, profile, join } = run;
        let running = self.running();
        let profile = profile.map_or_else(|| home.active(), str::to_owned);
        if self.profiles.lock().unwrap().contains(&profile) {
            return refused(format!("A game is already running as {profile}."));
        }
        // beside the shown game, an extra: its exit isn't the page's to show, unless it crashed
        let extra = running.is_some();
        let local = id == crate::dev::VERSION;
        let (dir, entry) = match local.then(crate::dev::build).flatten() {
            // the local build: run in place, and terra on PATH left as the last release's
            Some(local) => local,
            None => {
                let dir = home.version(id)?;
                let entry = dir.join(ENTRY);
                if !entry.is_file() {
                    return refused(format!("{id} isn't installed properly (it has no {ENTRY}). Remove it and install it again."));
                }
                // `terra <command>` is the CLI of the version being played
                point_cli(home, &dir)?;
                (dir, entry)
            }
        };

        let since = now_ms();
        let session = session_name(since);
        std::fs::create_dir_all(home.logs()).context(|| format!("creating {}", home.logs().display()))?;
        let log = log_path(home, &session);
        let mut out = std::fs::File::create(&log).context(|| format!("creating {}", log.display()))?;
        writeln!(out, "{LOG_HEADER}{id}").context(|| format!("writing {}", log.display()))?;
        let err = out.try_clone().context(|| format!("opening {}", log.display()))?;

        let mut command = tokio::process::Command::new(&entry);
        if let Some(appdir) = std::env::var_os("APPDIR") {
            for (key, value) in outside_appimage(std::env::vars_os(), std::path::Path::new(&appdir)) {
                match value {
                    Some(value) => command.env(key, value),
                    None => command.env_remove(key),
                };
            }
        }
        // a friend's world, joined once the game's signed in (shell/bridge/joins.h: --join id:handle)
        if let Some(join) = join {
            command.arg("--join").arg(join);
        }
        if let Some(page) = local.then(crate::dev::page).flatten() {
            command.arg("--shell-url").arg(page);
        }
        // the game signs in where the launcher does, so the key it finds is for that backend
        if api != crate::account::PRODUCTION_API {
            command.arg("--arclabs-url").arg(api);
        }
        // its own process group: the launcher closing, or a dev launcher restarting on a change,
        // doesn't take the game down with it (on Windows a child outlives its parent already)
        #[cfg(unix)]
        command.process_group(0);
        let mut child = command
            .current_dir(&dir)
            .env("TERRAVERSE_HOME", home.root())
            // the game points at the launcher for versions (release/release.h: launched)
            .env("TERRAVERSE_LAUNCHER", "1")
            .env("TERRAVERSE_PROFILE", &profile)
            .stdin(Stdio::null())
            .stdout(Stdio::from(out))
            .stderr(Stdio::from(err))
            .spawn()
            .map_err(|e| crate::failure::Failure::Refused(format!("{id} couldn't start: {e}")))?;
        self.profiles.lock().unwrap().push(profile.clone());
        if extra {
            playing(&self.game, &self.profiles, app);
        } else {
            let profiles = self.profiles.lock().unwrap().clone();
            set(&self.game, app, Game::Running { version: id.to_owned(), since, profiles });
        }

        let app = app.clone();
        let version = id.to_owned();
        let game = self.game.clone();
        let profiles = self.profiles.clone();
        tauri::async_runtime::spawn(async move {
            let status = child.wait().await;
            let code = match &status {
                Ok(status) => exit_code(status),
                Err(e) => {
                    eprintln!("launcher: lost the game: {e}");
                    -1
                }
            };
            profiles.lock().unwrap().retain(|p| *p != profile);
            let crashed = code != 0;
            // an extra's crash is offered once nothing else is shown: it's never dropped
            let shown_free = !matches!(*game.lock().unwrap(), Game::Running { .. });
            if !extra || (crashed && shown_free) {
                set(&game, &app, if crashed { Game::Crashed { version, session, code } } else { Game::Idle });
            } else {
                if crashed {
                    eprintln!("launcher: the game as {profile} crashed ({code}); its log is session {session}");
                }
                playing(&game, &profiles, &app);
            }
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

/// The shown game, told again with who's playing now: an extra started or stopped beside it.
fn playing(game: &Mutex<Game>, profiles: &Mutex<Vec<String>>, app: &tauri::AppHandle) {
    let now = profiles.lock().unwrap().clone();
    let next = match &*game.lock().unwrap() {
        Game::Running { version, since, .. } => Game::Running { version: version.clone(), since: *since, profiles: now },
        _ => return,
    };
    set(game, app, next);
}

fn set(game: &Mutex<Game>, app: &tauri::AppHandle, next: Game) {
    *game.lock().unwrap() = next.clone();
    events::send(app, Message::Game { game: next });
}

/// The exit code, or for a signal the shell's 128 + its number.
/// Points home's CLI at this version's terra, or takes it away for a version without one
/// (anything from before the CLI shipped): a `terra` that's from another version would
/// prepare folders for an engine that isn't the one played.
fn point_cli(home: &Home, version: &std::path::Path) -> Result<()> {
    let link = home.cli();
    let target = version.join("bin").join(if cfg!(windows) { "terra.exe" } else { "terra" });
    if let Some(dir) = link.parent() {
        std::fs::create_dir_all(dir).context(|| format!("creating {}", dir.display()))?;
    }
    match std::fs::symlink_metadata(&link) {
        Ok(_) => std::fs::remove_file(&link).context(|| format!("replacing {}", link.display()))?,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
        Err(e) => return Err(e).context(|| format!("reading {}", link.display())),
    }
    if !target.is_file() {
        return Ok(());
    }
    #[cfg(unix)]
    std::os::unix::fs::symlink(&target, &link).context(|| format!("linking {}", link.display()))?;
    // no symlinks without privileges on Windows: a copy, refreshed on every launch
    #[cfg(windows)]
    std::fs::copy(&target, &link).map(|_| ()).context(|| format!("copying {}", link.display()))?;
    Ok(())
}

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

/// What the launcher's AppImage set for itself, undone for the game. Its
/// AppRun puts the AppImage's own libraries (Ubuntu 22.04's libwayland-cursor,
/// libXcursor, ...) on LD_LIBRARY_PATH and its GTK hook points GTK, GIO and
/// XDG_DATA_DIRS inside the mount: the game inherited them, loaded those old
/// libraries in place of the desktop's, and its cursor drew nothing (v0.1.3).
/// Each change: a variable to set to a new value, or to remove (None).
fn outside_appimage(
    vars: impl Iterator<Item = (std::ffi::OsString, std::ffi::OsString)>,
    appdir: &std::path::Path,
) -> Vec<(std::ffi::OsString, Option<std::ffi::OsString>)> {
    // the AppImage's runtime and linuxdeploy's GTK hook, whatever their values
    const OURS: &[&str] = &["APPDIR", "APPIMAGE", "ARGV0", "OWD", "GTK_THEME", "WEBKIT_DISABLE_DMABUF_RENDERER"];
    let mut changes = Vec::new();
    for (key, value) in vars {
        if key.to_str().is_some_and(|k| OURS.contains(&k)) {
            changes.push((key, None));
            continue;
        }
        // path lists lose their entries inside the mount; a variable wholly inside it goes
        let paths: Vec<PathBuf> = std::env::split_paths(&value).collect();
        let kept: Vec<&PathBuf> = paths.iter().filter(|p| !p.starts_with(appdir)).collect();
        if kept.len() == paths.len() {
            continue;
        }
        let kept: Vec<&PathBuf> = kept.into_iter().filter(|p| !p.as_os_str().is_empty()).collect();
        if kept.is_empty() {
            changes.push((key, None));
        } else {
            changes.push((key, Some(std::env::join_paths(kept).expect("paths that were joined join again"))));
        }
    }
    changes
}

#[cfg(test)]
mod tests {
    // AppImages are Linux-only, and the path lists here are ':'-separated as
    // split_paths reads them on unix; on Windows it splits on ';' instead
    #[cfg(unix)]
    #[test]
    fn leaves_the_appimage_behind() {
        let vars = [
            ("LD_LIBRARY_PATH", "/tmp/.mount_X/usr/lib/:/tmp/.mount_X/lib64/:/opt/own"),
            ("XDG_DATA_DIRS", "/tmp/.mount_X/usr/share/:/usr/share:/usr/local/share"),
            ("GDK_PIXBUF_MODULE_FILE", "/tmp/.mount_X//usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"),
            ("GTK_THEME", "Adwaita:dark"),
            ("APPDIR", "/tmp/.mount_X"),
            ("HOME", "/home/someone"),
            ("XCURSOR_SIZE", "24"),
        ]
        .into_iter()
        .map(|(k, v)| (k.into(), v.into()));
        let mut changes: Vec<(String, Option<String>)> = super::outside_appimage(vars, std::path::Path::new("/tmp/.mount_X"))
            .into_iter()
            .map(|(k, v)| (k.into_string().unwrap(), v.map(|v| v.into_string().unwrap())))
            .collect();
        changes.sort();
        assert_eq!(
            changes,
            vec![
                ("APPDIR".into(), None),
                ("GDK_PIXBUF_MODULE_FILE".into(), None),
                ("GTK_THEME".into(), None),
                ("LD_LIBRARY_PATH".into(), Some("/opt/own".into())),
                ("XDG_DATA_DIRS".into(), Some("/usr/share:/usr/local/share".into())),
            ]
        );
    }

    use super::*;

    #[test]
    fn names_sessions_by_utc_time() {
        assert_eq!(session_name(0), "1970-01-01T00-00-00");
        assert_eq!(session_name(1_794_307_267_000), "2026-11-10T10-41-07");
        assert_eq!(session_name(951_782_400_000), "2000-02-29T00-00-00");
    }
}
