// A problem report: a session's log, the system it ran on and a crash dump if
// the game left one, with the player's note, sent to ArcLabs. Mirrors `Report`
// in src/backend/types.ts.

use crate::failure::{refused, Context, Result};
use crate::game::{self, LOG_HEADER};
use crate::home::Home;
use serde::Serialize;
use serde_json::json;
use std::path::PathBuf;

// the end of a log is where a crash is; the start says what ran
const LOG_LINES: usize = 400;

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Report {
    session: String,
    version: String,
    system: String,
    log: String,
    dump: bool,
}

/// The named session, or the last one when the player reports by hand.
pub fn collect(home: &Home, session: Option<&str>) -> Result<Report> {
    let session = match session {
        Some(session) => checked(session)?.to_owned(),
        None => match last_session(home)? {
            Some(session) => session,
            None => return refused("There's no game session to report yet: play once, then report what happened."),
        },
    };
    let path = game::log_path(home, &session);
    let text = std::fs::read_to_string(&path).context(|| format!("reading {}", path.display()))?;
    let version = text.lines().next().and_then(|l| l.strip_prefix(LOG_HEADER)).unwrap_or("unknown").to_owned();
    let lines: Vec<&str> = text.lines().collect();
    let log = lines[lines.len().saturating_sub(LOG_LINES)..].join("\n");
    Ok(Report { dump: dump(home, &session).is_some(), session, version, system: system(), log })
}

pub async fn submit(http: &reqwest::Client, api: &str, key: Option<String>, home: &Home, session: &str, note: &str) -> Result<()> {
    let report = collect(home, Some(session))?;
    let dump = match dump(home, session) {
        Some(path) => {
            let bytes = std::fs::read(&path).context(|| format!("reading {}", path.display()))?;
            Some(base64::Engine::encode(&base64::engine::general_purpose::STANDARD, bytes))
        }
        None => None,
    };
    let body = json!({
        "session": report.session, "version": report.version, "platform": crate::manifest::PLATFORM,
        "launcher": env!("CARGO_PKG_VERSION"), "system": report.system, "log": report.log,
        "note": note, "dump": dump,
    });
    let mut request = http.post(format!("{api}/api/terrafirma/reports")).json(&body);
    if let Some(key) = key {
        request = request.bearer_auth(key);
    }
    let kept = game::log_path(home, session);
    let Ok(answer) = request.send().await else {
        return refused(format!("Couldn't reach ArcLabs to send it. The log is kept at {}.", kept.display()));
    };
    let code = answer.status().as_u16();
    if code == 200 {
        return Ok(());
    }
    // ArcLabs words its refusals in statusMessage (a 429 says when to try again)
    let why = answer.json::<serde_json::Value>().await.ok().and_then(|b| b["statusMessage"].as_str().map(str::to_owned));
    let why = why.unwrap_or_else(|| format!("ArcLabs answered {code}"));
    refused(format!("The report wasn't sent: {why}. The log is kept at {}.", kept.display()))
}

fn last_session(home: &Home) -> Result<Option<String>> {
    let dir = home.logs();
    if !dir.exists() {
        return Ok(None);
    }
    let mut sessions: Vec<String> = std::fs::read_dir(&dir)
        .context(|| format!("reading {}", dir.display()))?
        .flatten()
        .filter_map(|e| e.file_name().to_str()?.strip_prefix("session-")?.strip_suffix(".log").map(str::to_owned))
        .collect();
    sessions.sort(); // named by time, so in order
    Ok(sessions.pop())
}

/// A crash dump the game wrote during this session (crashes/<session>*), if any.
fn dump(home: &Home, session: &str) -> Option<PathBuf> {
    std::fs::read_dir(home.crashes()).ok()?.flatten().map(|e| e.path()).find(|p| {
        p.file_name().and_then(|n| n.to_str()).is_some_and(|n| n.starts_with(session))
    })
}

fn system() -> String {
    use sysinfo::System;
    let mut sys = System::new();
    sys.refresh_memory();
    sys.refresh_cpu_all();
    let cpu = sys.cpus().first().map_or("unknown CPU".to_owned(), |c| c.brand().trim().to_owned());
    format!(
        "{} {} (kernel {}) · {} · {} GB",
        System::name().unwrap_or_else(|| "unknown OS".into()),
        System::os_version().unwrap_or_default(),
        System::kernel_version().unwrap_or_default(),
        cpu,
        sys.total_memory() / (1 << 30),
    )
}

/// Session names come back from the page: only their shape reaches a path.
fn checked(session: &str) -> Result<&str> {
    let ok = session.len() == 19 && session.chars().all(|c| c.is_ascii_digit() || c == '-' || c == 'T');
    if !ok {
        return refused(format!("Not a session: {session}"));
    }
    Ok(session)
}
