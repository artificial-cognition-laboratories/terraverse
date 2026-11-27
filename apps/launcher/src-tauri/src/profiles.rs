// The accounts this machine remembers: a profile named @handle holding a saved key for the
// backend in use, as the game lists them (shell/bridge/profiles.cpp). Switching makes one the
// active profile and reads its key; leaving goes back to `local` with the key kept, the way to
// add another account; forgetting deletes a key, never the profile's worlds.

use crate::account::{Account, PRODUCTION_API};
use crate::failure::{refused, Context, Result};
use crate::home::{valid_name, Home};
use serde::Serialize;

/// One remembered account, as the game's `RememberedProfile`.
#[derive(Serialize)]
pub struct Remembered {
    name: String,
    handle: String,
    current: bool,
}

impl Remembered {
    pub fn name(&self) -> &str {
        &self.name
    }
}

pub fn list(home: &Home, api: &str) -> Result<Vec<Remembered>> {
    let dir = home.root().join("profiles");
    let entries = match std::fs::read_dir(&dir) {
        Ok(entries) => entries,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(Vec::new()),
        Err(e) => return Err(e).context(|| format!("listing {}", dir.display())),
    };
    let active = home.active();
    let mut out = Vec::new();
    for entry in entries {
        let entry = entry.context(|| format!("listing {}", dir.display()))?;
        let Some(name) = entry.file_name().to_str().map(str::to_owned) else { continue };
        // an account's profile is @handle; others (local, a test player's) have no key to remember
        if !name.starts_with('@') || !valid_name(&name) || !remembers(home, &name, api) {
            continue;
        }
        out.push(Remembered { handle: name[1..].to_owned(), current: name == active, name });
    }
    out.sort_by(|a, b| a.handle.cmp(&b.handle));
    Ok(out)
}

pub fn use_(home: &Home, account: &Account, name: &str) -> Result<()> {
    if !name.starts_with('@') || !valid_name(name) || !remembers(home, name, account.api()) {
        return refused(format!("This machine doesn't remember {name}: sign in to add it."));
    }
    home.use_profile(name)?;
    account.reload();
    Ok(())
}

pub fn leave(home: &Home, account: &Account) -> Result<()> {
    home.signed_out()?;
    account.reload();
    Ok(())
}

pub fn forget(home: &Home, account: &Account, name: &str) -> Result<()> {
    if !name.starts_with('@') || !valid_name(name) {
        return refused(format!("No profile is called {name}."));
    }
    if name == home.active() && account.key().is_some() {
        return refused(format!("Sign out of {name} first."));
    }
    let key = home.key_in(name, account.api(), PRODUCTION_API);
    match std::fs::remove_file(&key) {
        Err(e) if e.kind() != std::io::ErrorKind::NotFound => Err(e).context(|| format!("deleting {}", key.display())),
        _ => Ok(()),
    }
}

fn remembers(home: &Home, name: &str, api: &str) -> bool {
    home.key_in(name, api, PRODUCTION_API).is_file()
}
