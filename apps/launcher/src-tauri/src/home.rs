// Where everything lives (.agents/planning/home.md): one root, shared with the game and terra,
// a profile per player, a cache that's safe to delete. The launcher's own files are named for
// it, so they never meet the game's.

use crate::failure::{refused, Context, Result};
use std::path::{Path, PathBuf};
use tauri::Manager;

pub struct Home {
    root: PathBuf,
}

/// What a player keeps, as the old layout kept it in the root: it all goes to `local`.
const PLAYER: &[&str] = &["worlds", "servers", "textures", "parts", "backups", "screenshots", "mod_data"];

impl Home {
    /// TERRAVERSE_HOME when it's set (as the game reads it), else this build's own place:
    /// a debug launcher never shares a home with the one players install.
    pub fn locate(app: &tauri::AppHandle) -> Result<Home> {
        let root = match std::env::var_os("TERRAVERSE_HOME").filter(|v| !v.is_empty()) {
            Some(home) => {
                let home = PathBuf::from(home);
                if !home.is_absolute() {
                    return refused("TERRAVERSE_HOME must be an absolute path.");
                }
                home
            }
            None if cfg!(windows) => app.path().data_dir().context(|| "finding the data folder".into())?.join(windows_name()),
            None => app.path().home_dir().context(|| "finding the home folder".into())?.join(unix_name()),
        };
        Home::at(root)
    }

    /// The home at `root`, made ready: what an older layout left there moved in, once.
    pub fn at(root: PathBuf) -> Result<Home> {
        std::fs::create_dir_all(root.join("cache")).context(|| format!("creating {}", root.display()))?;
        let home = Home { root };
        if !home.cache().join("home.json").exists() {
            home.move_in()?;
            std::fs::write(home.cache().join("home.json"), "{\n  \"layout\" : 1\n}\n").context(|| "marking the home moved in".into())?;
        }
        Ok(home)
    }

    pub fn root(&self) -> &Path {
        &self.root
    }
    /// Rebuildable: delete it and nothing is lost.
    pub fn cache(&self) -> PathBuf {
        self.root.join("cache")
    }
    pub fn settings(&self) -> PathBuf {
        self.root.join("launcher.json")
    }
    pub fn manifest(&self) -> PathBuf {
        self.cache().join("launcher-versions.json")
    }
    pub fn versions(&self) -> PathBuf {
        self.root.join("versions")
    }
    /// Half-done downloads and unpacks: never a version until renamed out.
    pub fn staging(&self) -> PathBuf {
        self.versions().join(".staging")
    }
    pub fn version(&self, id: &str) -> Result<PathBuf> {
        Ok(self.versions().join(checked(id)?))
    }
    /// The terra CLI of the version last played: what `terra <command>` on PATH runs
    /// (apps/game/scripts/release/install.sh writes those shims).
    pub fn cli(&self) -> PathBuf {
        self.root.join("bin").join(if cfg!(windows) { "terra.exe" } else { "terra" })
    }
    pub fn logs(&self) -> PathBuf {
        self.cache().join("logs")
    }
    pub fn crashes(&self) -> PathBuf {
        self.cache().join("crashes")
    }

    /// The profile running now: TERRAVERSE_PROFILE, then the remembered one, then local.
    pub fn active(&self) -> String {
        if let Some(name) = std::env::var("TERRAVERSE_PROFILE").ok().filter(|n| valid_name(n)) {
            return name;
        }
        std::fs::read_to_string(self.active_file())
            .ok()
            .and_then(|text| serde_json::from_str::<serde_json::Value>(&text).ok())
            .and_then(|v| v.get("profile").and_then(|p| p.as_str()).map(str::to_owned))
            .filter(|n| valid_name(n))
            .unwrap_or_else(|| "local".into())
    }

    /// A profile's folder, made if it's new.
    pub fn profile(&self, name: &str) -> Result<PathBuf> {
        if !valid_name(name) {
            return refused(format!("Not a profile's name: {name}"));
        }
        let dir = self.root.join("profiles").join(name);
        if !dir.exists() {
            std::fs::create_dir_all(&dir).context(|| format!("creating {}", dir.display()))?;
            let handle = name.strip_prefix('@');
            let profile = serde_json::json!({ "name": name, "handle": handle, "created": now() });
            write_json(&dir.join("profile.json"), &profile)?;
        }
        Ok(dir)
    }

    /// Run as `name` from now on, here and in the game.
    pub fn use_profile(&self, name: &str) -> Result<()> {
        self.profile(name)?;
        write_json(&self.active_file(), &serde_json::json!({ "profile": name }))
    }

    /// The game's saved sign-in in the active profile, one per backend (shell/shell.cpp: accountPath).
    pub fn account(&self, api: &str, default_api: &str) -> Result<PathBuf> {
        Ok(self.profile(&self.active())?.join(account_file(api, default_api)))
    }

    /// Where a profile keeps its saved sign-in for a backend, whether or not it's there.
    pub fn key_in(&self, name: &str, api: &str, default_api: &str) -> PathBuf {
        self.root.join("profiles").join(name).join(account_file(api, default_api))
    }

    /// An account signed in: its profile becomes the active one, adopting `local` when it has
    /// none yet, and the key file moves into it. Its new path.
    pub fn signed_in(&self, handle: &str, key_file: &Path) -> Result<PathBuf> {
        let name = format!("@{handle}");
        if !valid_name(&name) {
            return refused(format!("ArcLabs named the account {handle}, which can't name a profile."));
        }
        let from = self.active();
        let file = key_file.file_name().map(PathBuf::from).unwrap_or_default();
        if from == name {
            return Ok(key_file.to_owned());
        }
        let dir = self.root.join("profiles").join(&name);
        if from == "local" && !dir.exists() {
            // the first sign-in here: what was played signed out becomes the account's
            let local = self.profile("local")?;
            std::fs::rename(&local, &dir).context(|| format!("moving {} to {}", local.display(), dir.display()))?;
            write_json(&dir.join("profile.json"), &serde_json::json!({ "name": name, "handle": handle, "created": now() }))?;
            self.use_profile(&name)?;
            return Ok(dir.join(file));
        }
        self.use_profile(&name)?;
        let to = self.profile(&name)?.join(file);
        if key_file.exists() {
            std::fs::rename(key_file, &to).context(|| format!("moving {} to {}", key_file.display(), to.display()))?;
        }
        Ok(to)
    }

    /// Signed out: back to local; the account's profile is kept for next time.
    pub fn signed_out(&self) -> Result<()> {
        self.use_profile("local")
    }

    fn active_file(&self) -> PathBuf {
        self.cache().join("active-profile.json")
    }

    /// The old layout, which kept everything in the root (home.md, "Moving in"). Safe to repeat.
    fn move_in(&self) -> Result<()> {
        let local = self.root.join("profiles").join("local");
        std::fs::create_dir_all(&local).context(|| format!("creating {}", local.display()))?;
        let mut moved = Vec::new();
        let mut take = |from: PathBuf, to: PathBuf| -> Result<()> {
            if from.exists() && !to.exists() {
                std::fs::rename(&from, &to).context(|| format!("moving {} to {}", from.display(), to.display()))?;
                moved.push(format!("- {}  →  {}", from.display(), to.display()));
            }
            Ok(())
        };
        for name in PLAYER {
            take(self.root.join(name), local.join(name))?;
        }
        if let Ok(entries) = std::fs::read_dir(&self.root) {
            for entry in entries.flatten() {
                let name = entry.file_name().to_string_lossy().into_owned();
                if name.starts_with("account") && name.ends_with(".json") {
                    take(entry.path(), local.join(&name))?;
                }
            }
        }
        take(self.root.join("minetest.conf"), self.root.join("settings.conf"))?;
        take(self.root.join("launcher-versions.json"), self.manifest())?;
        take(self.root.join("logs"), self.logs())?;
        take(self.root.join("crashes"), self.crashes())?;
        if !moved.is_empty() {
            let note = format!(
                "# Moved\n\nTerraverse keeps everything in one place, laid out by profile now (.agents/planning/home.md in the Terraverse repo):\n\n{}\n",
                moved.join("\n")
            );
            std::fs::write(self.root.join("MOVED.md"), note).context(|| "writing MOVED.md".into())?;
        }
        Ok(())
    }
}

/// A profile's name: local, @handle, or a plain test player's ("two").
pub fn valid_name(name: &str) -> bool {
    let (plain, handle) = match name.strip_prefix('@') {
        Some(rest) => (rest, true),
        None => (name, false),
    };
    !plain.is_empty()
        && plain.len() <= 64
        && plain != "."
        && plain != ".."
        && plain.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '-' || (handle && c == '.'))
}

/// The game's key file for a backend: production's is account.json, any other's is named by its host.
fn account_file(api: &str, default_api: &str) -> String {
    if api == default_api {
        return "account.json".into();
    }
    let host: String = api
        .split_once("://")
        .map_or(api, |(_, rest)| rest)
        .chars()
        .map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' { c } else { '_' })
        .collect();
    format!("account-{host}.json")
}

fn unix_name() -> &'static str {
    if cfg!(debug_assertions) { ".terraverse-dev" } else { ".terraverse" }
}

fn windows_name() -> &'static str {
    if cfg!(debug_assertions) { "Terraverse-dev" } else { "Terraverse" }
}

fn now() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0)
}

fn write_json(path: &Path, value: &serde_json::Value) -> Result<()> {
    let text = serde_json::to_string_pretty(value).context(|| "writing JSON".into())?;
    std::fs::write(path, text + "\n").context(|| format!("writing {}", path.display()))
}

/// A version id goes into paths: only the shape of one gets there.
pub fn checked(id: &str) -> Result<&str> {
    let ok = !id.is_empty()
        && id.len() <= 64
        && !id.starts_with('.')
        && id.chars().all(|c| c.is_ascii_alphanumeric() || c == '.' || c == '-');
    if !ok {
        return refused(format!("Not a version: {id}"));
    }
    Ok(id)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fresh() -> (tempfile::TempDir, Home) {
        let dir = tempfile::tempdir().unwrap();
        let home = Home::at(dir.path().to_owned()).unwrap();
        (dir, home)
    }

    #[test]
    fn version_ids_stay_in_their_folder() {
        assert!(checked("0.3.0-beta.2").is_ok());
        for bad in ["", "..", ".staging", "../x", "a/b", "a\\b"] {
            assert!(checked(bad).is_err(), "{bad}");
        }
    }

    #[test]
    fn an_old_layout_moves_in() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::create_dir_all(dir.path().join("worlds/mine")).unwrap();
        std::fs::write(dir.path().join("account.json"), "{\"key\":\"k\"}").unwrap();
        std::fs::create_dir_all(dir.path().join("logs")).unwrap();
        let home = Home::at(dir.path().to_owned()).unwrap();
        assert!(home.root().join("profiles/local/worlds/mine").exists());
        assert!(home.root().join("profiles/local/account.json").exists());
        assert!(home.logs().exists());
        assert!(home.root().join("MOVED.md").exists());
        assert_eq!(home.account("https://p", "https://p").unwrap(), home.root().join("profiles/local/account.json"));
    }

    #[test]
    fn the_first_sign_in_adopts_local_and_signing_out_returns() {
        let (_dir, home) = fresh();
        let key = home.account("https://p", "https://p").unwrap();
        std::fs::write(&key, "{}").unwrap();
        std::fs::create_dir_all(home.root().join("profiles/local/worlds/mine")).unwrap();
        let moved = home.signed_in("cody", &key).unwrap();
        assert_eq!(home.active(), "@cody");
        assert!(moved.exists());
        assert!(home.root().join("profiles/@cody/worlds/mine").exists());
        home.signed_out().unwrap();
        assert_eq!(home.active(), "local");
        assert!(home.root().join("profiles/@cody").exists());
    }

    #[test]
    fn names_are_checked() {
        for good in ["local", "@cody", "@cody.w", "two"] {
            assert!(valid_name(good), "{good}");
        }
        for bad in ["", "@", "../x", "a/b", "@.."] {
            assert!(!valid_name(bad), "{bad}");
        }
    }
}
