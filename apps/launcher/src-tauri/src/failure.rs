// What a command answers when it doesn't succeed. A refusal is an expected no,
// worded for the player; a bug is anything else, and the page throws it rather
// than show it as an answer (src/backend/tauri.ts).

use serde::Serialize;
use std::fmt;

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum Failure {
    Refused(String),
    Bug(String),
}

pub type Result<T> = std::result::Result<T, Failure>;

pub fn refused<T>(why: impl Into<String>) -> Result<T> {
    Err(Failure::Refused(why.into()))
}

impl Failure {
    /// The words for the player, whichever kind it is: for background work,
    /// where there's no caller to throw to.
    pub fn words(&self) -> &str {
        match self {
            Failure::Refused(why) | Failure::Bug(why) => why,
        }
    }
}

impl fmt::Display for Failure {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.words())
    }
}

/// Context for an error that shouldn't happen: what was being done, then why it failed.
pub trait Context<T> {
    fn context(self, what: impl FnOnce() -> String) -> Result<T>;
}

impl<T, E: fmt::Display> Context<T> for std::result::Result<T, E> {
    fn context(self, what: impl FnOnce() -> String) -> Result<T> {
        self.map_err(|e| Failure::Bug(format!("{}: {e}", what())))
    }
}
