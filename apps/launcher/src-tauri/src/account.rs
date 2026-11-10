// The player's ArcLabs account, as the game keeps it (shell/account.cpp): the
// backend's device flow (client "terrafirma"), approved in the browser, and
// the key it gives saved to the same account.json. Signing in here or in the
// game signs in both. Every change goes to the page as an `account` message,
// in the game's shape (ui/src/bridge/types.ts: Account).

use crate::events::{self, Message};
use crate::failure::{refused, Context, Result};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};
use tauri_plugin_opener::OpenerExt;

/// The game's default (defaultsettings.cpp: arclabs_url); TERRAVERSE_ARCLABS_URL for another.
pub const DEFAULT_API: &str = "https://axon-api-t53zrgvpga-ew.a.run.app";

#[derive(Clone, Debug, Serialize)]
#[serde(tag = "status", rename_all = "camelCase")]
pub enum View {
    SignedOut {
        #[serde(skip_serializing_if = "Option::is_none")]
        problem: Option<String>,
    },
    Starting,
    Pending { code: String, url: String },
    Checking,
    SignedIn { id: String, username: String, name: Option<String> },
    Unreachable { problem: String },
}

struct State {
    view: View,
    key: Option<String>,
    // bumped whenever a flow is abandoned: answers to older requests are dropped
    generation: u64,
}

#[derive(Clone)]
pub struct Account {
    api: String,
    key_path: PathBuf,
    http: reqwest::Client,
    app: tauri::AppHandle,
    state: Arc<Mutex<State>>,
    /// Told the usable key (None unless signed in) on every change: friends follow it.
    changed: Arc<dyn Fn(Option<String>) + Send + Sync>,
}

#[derive(Serialize, Deserialize)]
struct Saved {
    key: String,
}

#[derive(Deserialize)]
struct Authorization {
    device_code: String,
    user_code: String,
    verification_uri_complete: String,
    interval: u64,
    expires_in: u64,
}

#[derive(Deserialize)]
struct Poll {
    status: String,
    access_token: Option<String>,
}

#[derive(Deserialize)]
struct Me {
    id: String,
    username: String,
    name: Option<String>,
}

impl Account {
    pub fn new(api: String, key_path: PathBuf, http: reqwest::Client, app: tauri::AppHandle, changed: Arc<dyn Fn(Option<String>) + Send + Sync>) -> Account {
        let account = Account {
            api,
            key_path,
            http,
            app,
            state: Arc::new(Mutex::new(State { view: View::SignedOut { problem: None }, key: None, generation: 0 })),
            changed,
        };
        account.reload();
        account
    }

    pub fn api(&self) -> &str {
        &self.api
    }

    pub fn view(&self) -> View {
        self.state.lock().unwrap().view.clone()
    }

    /// The key, while signed in: for the other ArcLabs callers.
    pub fn key(&self) -> Option<String> {
        let state = self.state.lock().unwrap();
        matches!(state.view, View::SignedIn { .. }).then(|| state.key.clone()).flatten()
    }

    /// Reads the saved key again: at start, and after the game has run (it may
    /// have signed in or out meanwhile).
    pub fn reload(&self) {
        let saved = match std::fs::read_to_string(&self.key_path) {
            Ok(text) => match serde_json::from_str::<Saved>(&text) {
                Ok(saved) => Some(saved.key),
                Err(e) => {
                    eprintln!("launcher: {} is damaged: {e}", self.key_path.display());
                    None
                }
            },
            Err(_) => None,
        };
        let same = self.state.lock().unwrap().key == saved;
        if same {
            return;
        }
        match saved {
            Some(key) => {
                self.state.lock().unwrap().key = Some(key);
                self.check();
            }
            None => {
                let mut state = self.state.lock().unwrap();
                state.key = None;
                state.generation += 1;
                drop(state);
                self.become_(View::SignedOut { problem: None });
            }
        }
    }

    pub fn sign_in(&self) -> Result<()> {
        let generation = {
            let mut state = self.state.lock().unwrap();
            if !matches!(state.view, View::SignedOut { .. }) {
                return refused("Already signed in, or signing in.");
            }
            state.generation += 1;
            state.generation
        };
        self.become_(View::Starting);
        let account = self.clone();
        tauri::async_runtime::spawn(async move { account.device_flow(generation).await });
        Ok(())
    }

    pub fn cancel(&self) -> Result<()> {
        let mut state = self.state.lock().unwrap();
        if !matches!(state.view, View::Starting | View::Pending { .. }) {
            return refused("Not signing in.");
        }
        state.generation += 1;
        drop(state);
        self.become_(View::SignedOut { problem: None });
        Ok(())
    }

    pub fn sign_out(&self) -> Result<()> {
        let mut state = self.state.lock().unwrap();
        if matches!(state.view, View::SignedOut { .. }) {
            return refused("Not signed in.");
        }
        state.generation += 1;
        state.key = None;
        drop(state);
        if self.key_path.exists() {
            std::fs::remove_file(&self.key_path).context(|| format!("deleting {}", self.key_path.display()))?;
        }
        self.become_(View::SignedOut { problem: None });
        Ok(())
    }

    pub fn retry(&self) -> Result<()> {
        if !matches!(self.state.lock().unwrap().view, View::Unreachable { .. }) {
            return refused("Nothing to retry.");
        }
        self.check();
        Ok(())
    }

    fn become_(&self, view: View) {
        self.state.lock().unwrap().view = view.clone();
        let account = serde_json::to_value(&view).expect("an account view serialises");
        events::send(&self.app, Message::Account { account });
        (self.changed)(self.key());
    }

    fn current(&self, generation: u64) -> bool {
        self.state.lock().unwrap().generation == generation
    }

    /// Asks who the saved key belongs to.
    fn check(&self) {
        let generation = self.state.lock().unwrap().generation;
        self.become_(View::Checking);
        let account = self.clone();
        tauri::async_runtime::spawn(async move {
            let view = account.me().await;
            if account.current(generation) {
                if matches!(view, View::SignedOut { .. }) {
                    account.forget();
                }
                account.become_(view);
            }
        });
    }

    async fn me(&self) -> View {
        let Some(key) = self.state.lock().unwrap().key.clone() else {
            return View::SignedOut { problem: None };
        };
        let answer = self.http.get(format!("{}/api/terrafirma/me", self.api)).bearer_auth(key).send().await;
        let answer = match answer {
            Ok(answer) => answer,
            Err(_) => return View::Unreachable { problem: format!("couldn't reach {}", self.api) },
        };
        match answer.status().as_u16() {
            200 => match answer.json::<Me>().await {
                Ok(me) => View::SignedIn { id: me.id, username: me.username, name: me.name },
                Err(e) => View::Unreachable { problem: format!("ArcLabs answered strangely: {e}") },
            },
            401 | 403 => View::SignedOut { problem: Some("Your sign-in was revoked or has expired. Sign in again.".into()) },
            code => View::Unreachable { problem: format!("the ArcLabs backend failed ({code})") },
        }
    }

    fn forget(&self) {
        self.state.lock().unwrap().key = None;
        if self.key_path.exists() {
            if let Err(e) = std::fs::remove_file(&self.key_path) {
                eprintln!("launcher: couldn't delete {}: {e}", self.key_path.display());
            }
        }
    }

    async fn device_flow(&self, generation: u64) {
        if let Err(problem) = self.try_device_flow(generation).await {
            if self.current(generation) {
                self.become_(View::SignedOut { problem: Some(problem) });
            }
        }
    }

    /// Err is what the player is told; Ok also when abandoned.
    async fn try_device_flow(&self, generation: u64) -> std::result::Result<(), String> {
        let stopped = |e: reqwest::Error| format!("Sign-in stopped: couldn't reach {} ({e}).", self.api);
        let answer = self
            .http
            .post(format!("{}/api/user/device/authorize", self.api))
            .json(&serde_json::json!({ "client": "terrafirma" }))
            .send()
            .await
            .map_err(stopped)?;
        if answer.status().as_u16() != 200 {
            return Err(format!("Sign-in was refused by ArcLabs ({}). Try again.", answer.status().as_u16()));
        }
        let auth: Authorization = answer.json().await.map_err(|e| format!("Sign-in stopped: ArcLabs answered strangely ({e})."))?;
        if !self.current(generation) {
            return Ok(());
        }
        self.become_(View::Pending { code: auth.user_code.clone(), url: auth.verification_uri_complete.clone() });
        if let Err(e) = self.app.opener().open_url(&auth.verification_uri_complete, None::<&str>) {
            eprintln!("launcher: couldn't open the browser ({e}); the page shows the link");
        }

        let expires = Instant::now() + Duration::from_secs(auth.expires_in);
        let interval = Duration::from_secs(auth.interval.max(1));
        loop {
            tokio::time::sleep(interval).await;
            if !self.current(generation) {
                return Ok(());
            }
            if Instant::now() >= expires {
                return Err("The sign-in code expired. Try again.".into());
            }
            let answer = self
                .http
                .get(format!("{}/api/user/device/poll", self.api))
                .query(&[("device_code", &auth.device_code)])
                .send()
                .await
                .map_err(stopped)?;
            if answer.status().as_u16() != 200 {
                return Err(format!("Sign-in was refused by ArcLabs ({}). Try again.", answer.status().as_u16()));
            }
            let poll: Poll = answer.json().await.map_err(|e| format!("Sign-in stopped: ArcLabs answered strangely ({e})."))?;
            match (poll.status.as_str(), poll.access_token) {
                ("pending", _) => continue,
                ("expired", _) => return Err("The sign-in code expired. Try again.".into()),
                ("approved", Some(key)) => {
                    if !self.current(generation) {
                        return Ok(());
                    }
                    self.save(&key).map_err(|e| format!("Signed in, but couldn't save it: {e}"))?;
                    self.state.lock().unwrap().key = Some(key);
                    self.check();
                    return Ok(());
                }
                (status, _) => return Err(format!("Sign-in stopped: ArcLabs said \"{status}\".")),
            }
        }
    }

    fn save(&self, key: &str) -> Result<()> {
        use std::io::Write;
        let text = serde_json::to_string(&Saved { key: key.to_owned() }).context(|| "writing the account".into())?;
        let partial = self.key_path.with_extension("partial");
        let mut options = std::fs::OpenOptions::new();
        options.write(true).create(true).truncate(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options.mode(0o600); // it's a credential: private from its first byte
        }
        let mut file = options.open(&partial).context(|| format!("creating {}", partial.display()))?;
        file.write_all(text.as_bytes()).context(|| format!("writing {}", partial.display()))?;
        drop(file);
        std::fs::rename(&partial, &self.key_path).context(|| format!("saving {}", self.key_path.display()))
    }
}
