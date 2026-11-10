// Where everything lives: TERRAVERSE_HOME, which the game uses too
// (planning/launcher.md, "On disk"). The launcher's own files are named for it,
// so they never meet the game's.

use crate::failure::{refused, Context, Result};
use std::path::{Path, PathBuf};
use tauri::Manager;

pub struct Home {
    root: PathBuf,
}

impl Home {
    /// TERRAVERSE_HOME when it's set (as the game reads it), else the platform's place.
    pub fn locate(app: &tauri::AppHandle) -> Result<Home> {
        let root = match std::env::var_os("TERRAVERSE_HOME").filter(|v| !v.is_empty()) {
            Some(home) => {
                let home = PathBuf::from(home);
                if !home.is_absolute() {
                    return refused("TERRAVERSE_HOME must be an absolute path.");
                }
                home
            }
            None if cfg!(windows) => app.path().data_dir().context(|| "finding the data folder".into())?.join("Terraverse"),
            None => app.path().home_dir().context(|| "finding the home folder".into())?.join(".terraverse"),
        };
        std::fs::create_dir_all(&root).context(|| format!("creating {}", root.display()))?;
        Ok(Home { root })
    }

    pub fn root(&self) -> &Path {
        &self.root
    }
    pub fn settings(&self) -> PathBuf {
        self.root.join("launcher.json")
    }
    pub fn manifest(&self) -> PathBuf {
        self.root.join("launcher-versions.json")
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
    pub fn logs(&self) -> PathBuf {
        self.root.join("logs")
    }
    pub fn crashes(&self) -> PathBuf {
        self.root.join("crashes")
    }
    /// The game's saved sign-in, one per backend (shell/shell.cpp: accountPath).
    pub fn account(&self, api: &str, default_api: &str) -> PathBuf {
        if api == default_api {
            return self.root.join("account.json");
        }
        let host: String = api
            .split_once("://")
            .map_or(api, |(_, rest)| rest)
            .chars()
            .map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' { c } else { '_' })
            .collect();
        self.root.join(format!("account-{host}.json"))
    }
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

    #[test]
    fn version_ids_stay_in_their_folder() {
        assert!(checked("0.3.0-beta.2").is_ok());
        for bad in ["", "..", ".staging", "../x", "a/b", "a\\b"] {
            assert!(checked(bad).is_err(), "{bad}");
        }
    }
}
