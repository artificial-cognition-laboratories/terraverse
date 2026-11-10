// Friends, as the game shows them (shell/social.cpp): the view polled from
// ArcLabs while signed in, and the friend requests the game's dropdown makes.
// The launcher sends no presence: it isn't anywhere a friend could join.

use crate::events::{self, Message};
use crate::failure::{refused, Result};
use serde_json::{json, Value};
use std::sync::{Arc, Mutex};
use std::time::Duration;
use tokio::sync::Notify;

// as the game polls it (social.cpp: VIEW_MS)
const VIEW: Duration = Duration::from_secs(15);

#[derive(Clone)]
pub struct Social {
    api: String,
    http: reqwest::Client,
    app: tauri::AppHandle,
    state: Arc<Mutex<State>>,
    poke: Arc<Notify>,
}

#[derive(Default)]
struct State {
    key: Option<String>,
    view: Option<Value>, // null until the backend first answers
    problem: Option<String>,
}

impl Social {
    pub fn new(api: String, http: reqwest::Client, app: tauri::AppHandle) -> Social {
        let social = Social { api, http, app, state: Arc::default(), poke: Arc::new(Notify::new()) };
        let polling = social.clone();
        tauri::async_runtime::spawn(async move { polling.poll().await });
        social
    }

    /// The account's key now (None when signed out): the view follows it.
    pub fn signed_in(&self, key: Option<String>) {
        let mut state = self.state.lock().unwrap();
        if state.key == key {
            return;
        }
        state.key = key;
        state.view = None;
        state.problem = None;
        drop(state);
        self.publish();
        self.poke.notify_one();
    }

    /// As the page sees it (ui/src/bridge/types.ts: Social): null while signed out.
    pub fn get(&self) -> Value {
        let state = self.state.lock().unwrap();
        if state.key.is_none() {
            return Value::Null;
        }
        let mut out = json!({
            "me": { "state": "menu", "world": null, "open": false },
            "joinable": false,
            "view": state.view.clone().unwrap_or(Value::Null),
        });
        if let Some(problem) = &state.problem {
            out["problem"] = json!(problem);
        }
        out
    }

    fn publish(&self) {
        events::send(&self.app, Message::Social { social: self.get() });
    }

    async fn poll(&self) {
        loop {
            let key = self.state.lock().unwrap().key.clone();
            if let Some(key) = key {
                let answer = self.fetch(&key).await;
                let mut state = self.state.lock().unwrap();
                // signed out or in as someone else meanwhile: this answer is stale
                if state.key.as_deref() == Some(key.as_str()) {
                    let changed = match answer {
                        Ok(view) => {
                            let changed = state.view.as_ref() != Some(&view) || state.problem.is_some();
                            state.view = Some(view);
                            state.problem = None;
                            changed
                        }
                        Err(why) => {
                            let changed = state.problem.as_deref() != Some(why.as_str());
                            state.problem = Some(why);
                            changed
                        }
                    };
                    drop(state);
                    if changed {
                        self.publish();
                    }
                }
            }
            let _ = tokio::time::timeout(VIEW, self.poke.notified()).await;
        }
    }

    async fn fetch(&self, key: &str) -> std::result::Result<Value, String> {
        let answer = self.http.get(format!("{}/api/terrafirma/social", self.api)).bearer_auth(key).send().await;
        let answer = answer.map_err(|_| "couldn't reach ArcLabs".to_owned())?;
        let code = answer.status().as_u16();
        let body: Value = answer.json().await.map_err(|e| format!("ArcLabs answered strangely: {e}"))?;
        if code != 200 {
            return Err(reason(code, &body));
        }
        for list in ["friends", "incoming", "outgoing", "invites"] {
            if !body[list].is_array() {
                return Err(format!("the ArcLabs friends view has no \"{list}\" list"));
            }
        }
        Ok(body)
    }

    pub async fn request(&self, username: &str) -> Result<()> {
        self.call(reqwest::Method::POST, "/api/terrafirma/friends".into(), Some(json!({ "username": username }))).await
    }
    pub async fn accept(&self, id: &str) -> Result<()> {
        self.call(reqwest::Method::POST, format!("/api/terrafirma/friends/{}/accept", checked(id)?), None).await
    }
    pub async fn remove(&self, id: &str) -> Result<()> {
        self.call(reqwest::Method::DELETE, format!("/api/terrafirma/friends/{}", checked(id)?), None).await
    }

    async fn call(&self, method: reqwest::Method, path: String, body: Option<Value>) -> Result<()> {
        let Some(key) = self.state.lock().unwrap().key.clone() else {
            return refused("Sign in to ArcLabs first.");
        };
        let mut request = self.http.request(method, format!("{}{path}", self.api)).bearer_auth(key);
        if let Some(body) = body {
            request = request.json(&body);
        }
        let Ok(answer) = request.send().await else {
            return refused("Couldn't reach ArcLabs. Check your connection.");
        };
        let code = answer.status().as_u16();
        if code != 200 {
            let body = answer.json::<Value>().await.unwrap_or(Value::Null);
            if code >= 500 {
                return Err(crate::failure::Failure::Bug(format!("{path} failed ({code}): {body}")));
            }
            return refused(reason(code, &body));
        }
        self.poke.notify_one(); // show the change without waiting for the next poll
        Ok(())
    }
}

/// ArcLabs words its refusals in statusMessage (online/http.cpp: reason).
fn reason(code: u16, body: &Value) -> String {
    body["statusMessage"].as_str().map_or_else(|| format!("ArcLabs answered {code}"), str::to_owned)
}

/// Ids from the page go into URL paths: only the shape of a uuid gets there.
fn checked(id: &str) -> Result<&str> {
    let ok = id.len() == 36
        && id.char_indices().all(|(i, c)| if matches!(i, 8 | 13 | 18 | 23) { c == '-' } else { c.is_ascii_hexdigit() });
    if !ok {
        return refused(format!("Not a player id: {id}"));
    }
    Ok(id)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_uuids_reach_paths() {
        assert!(checked("123e4567-e89b-12d3-a456-426614174000").is_ok());
        assert!(checked("../../admin").is_err());
        assert!(checked("123e4567-e89b-12d3-a456-42661417400/").is_err());
    }
}
