// What the launcher remembers between runs (launcher.json). Mirrors
// `Settings` in src/backend/types.ts.

use crate::failure::{Context, Result};
use crate::home;
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum OnLaunch {
    Hide,
    Close,
    Stay,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Settings {
    /// Betas count as the newest when nothing's pinned.
    pub beta: bool,
    pub pinned: Option<String>,
    pub on_launch: OnLaunch,
    /// Off: older versions go once a newer one has been played.
    pub keep_old: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Settings { beta: false, pinned: None, on_launch: OnLaunch::Hide, keep_old: true }
    }
}

impl Settings {
    pub fn load(path: &Path) -> Result<Settings> {
        if !path.exists() {
            return Ok(Settings::default());
        }
        let text = std::fs::read_to_string(path).context(|| format!("reading {}", path.display()))?;
        let settings: Settings = serde_json::from_str(&text).context(|| format!("{} is damaged", path.display()))?;
        settings.validated()
    }

    pub fn save(&self, path: &Path) -> Result<()> {
        let text = serde_json::to_string_pretty(self).context(|| "writing the settings".into())?;
        write_atomically(path, text.as_bytes())
    }

    /// From the page or the disk: a pinned version must be one.
    pub fn validated(self) -> Result<Settings> {
        if let Some(pinned) = &self.pinned {
            home::checked(pinned)?;
        }
        Ok(self)
    }
}

/// Written beside, then renamed over: a crash never leaves half a file.
pub fn write_atomically(path: &Path, bytes: &[u8]) -> Result<()> {
    let partial = path.with_extension("partial");
    std::fs::write(&partial, bytes).context(|| format!("writing {}", partial.display()))?;
    std::fs::rename(&partial, path).context(|| format!("replacing {}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips_and_defaults() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("launcher.json");
        assert_eq!(Settings::load(&path).unwrap(), Settings::default());
        let changed = Settings { beta: true, pinned: Some("0.2.0".into()), on_launch: OnLaunch::Stay, keep_old: false };
        changed.save(&path).unwrap();
        assert_eq!(Settings::load(&path).unwrap(), changed);
    }

    #[test]
    fn refuses_a_damaged_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("launcher.json");
        std::fs::write(&path, r#"{"beta":true}"#).unwrap();
        assert!(Settings::load(&path).is_err());
        std::fs::write(&path, r#"{"beta":true,"pinned":"../x","onLaunch":"hide","keepOld":true}"#).unwrap();
        assert!(Settings::load(&path).is_err());
    }
}
