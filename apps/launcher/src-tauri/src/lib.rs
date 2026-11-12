// The launcher's backend: the commands the page calls. Mirror
// `src/backend/types.ts` and `src/backend/tauri.ts`: change them together.
// Each concern is its own module; this file only holds them and answers.

mod account;
mod events;
mod failure;
mod game;
mod home;
mod install;
mod manifest;
mod report;
mod settings;
mod signature;
mod social;
mod update;
mod version;

use failure::{refused, Context, Result};
use home::Home;
use manifest::Manifest;
use serde::Serialize;
use settings::{OnLaunch, Settings};
use std::sync::{Arc, Mutex};
use tauri::{Manager, State};
use tauri_plugin_opener::OpenerExt;

struct Launcher {
    app: tauri::AppHandle,
    home: Arc<Home>,
    http: reqwest::Client,
    settings: Mutex<Settings>,
    manifest: Mutex<Option<Manifest>>,
    installs: Arc<install::Installs>,
    runner: game::Runner,
    account: account::Account,
    social: social::Social,
    updates: update::Updates,
}

type L<'a> = State<'a, Arc<Launcher>>;

impl Launcher {
    fn new(app: &tauri::AppHandle) -> Result<Launcher> {
        let home = Arc::new(Home::locate(app)?);
        let http = reqwest::Client::builder()
            .user_agent(concat!("terraverse-launcher/", env!("CARGO_PKG_VERSION")))
            .connect_timeout(std::time::Duration::from_secs(5))
            .build()
            .context(|| "starting the HTTP client".into())?;
        let api = std::env::var("TERRAVERSE_ARCLABS_URL").unwrap_or_else(|_| account::DEFAULT_API.to_owned());
        let social = social::Social::new(api.clone(), http.clone(), app.clone());
        let follows = social.clone();
        let account = account::Account::new(
            api.clone(),
            home.account(&api, account::DEFAULT_API),
            http.clone(),
            app.clone(),
            Arc::new(move |key| follows.signed_in(key)),
        );
        social.signed_in(account.key());
        Ok(Launcher {
            app: app.clone(),
            settings: Mutex::new(Settings::load(&home.settings())?),
            manifest: Mutex::new(Manifest::load(&home.manifest())?),
            home,
            http,
            installs: Arc::default(),
            runner: game::Runner::new(),
            account,
            social,
            updates: update::Updates::default(),
        })
    }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct About {
    launcher: &'static str,
    data_dir: String,
    platform: &'static str,
    free: u64,
}

#[tauri::command]
fn about(l: L) -> About {
    let root = l.home.root();
    // the disk the data folder is on: the longest mount point it's under
    let disks = sysinfo::Disks::new_with_refreshed_list();
    let free = disks
        .iter()
        .filter(|d| root.starts_with(d.mount_point()))
        .max_by_key(|d| d.mount_point().as_os_str().len())
        .map_or(0, |d| d.available_space());
    About { launcher: env!("CARGO_PKG_VERSION"), data_dir: root.display().to_string(), platform: manifest::PLATFORM, free }
}

#[tauri::command]
fn account(l: L) -> account::View {
    l.account.view()
}
#[tauri::command]
fn sign_in(l: L) -> Result<()> {
    l.account.sign_in()
}
#[tauri::command]
fn cancel_sign_in(l: L) -> Result<()> {
    l.account.cancel()
}
#[tauri::command]
fn retry_sign_in(l: L) -> Result<()> {
    l.account.retry()
}
#[tauri::command]
fn sign_out(l: L) -> Result<()> {
    if l.runner.running().is_some() {
        return refused("Close the game before signing out: it's using the account.");
    }
    l.account.sign_out()
}

#[tauri::command]
fn social(l: L) -> serde_json::Value {
    l.social.get()
}
#[tauri::command]
async fn request_friend(l: L<'_>, username: String) -> Result<()> {
    l.social.request(username.trim()).await
}
#[tauri::command]
async fn accept_friend(l: L<'_>, id: String) -> Result<()> {
    l.social.accept(&id).await
}
#[tauri::command]
async fn remove_friend(l: L<'_>, id: String) -> Result<()> {
    l.social.remove(&id).await
}

#[tauri::command]
fn settings(l: L) -> Settings {
    l.settings.lock().unwrap().clone()
}
#[tauri::command]
fn set_settings(l: L, settings: Settings) -> Result<Settings> {
    let settings = settings.validated()?;
    settings.save(&l.home.settings())?;
    *l.settings.lock().unwrap() = settings.clone();
    Ok(settings)
}

#[tauri::command]
async fn refresh_manifest(l: L<'_>) -> Result<serde_json::Value> {
    let fresh = manifest::fetch(&l.http).await?;
    fresh.save(&l.home.manifest())?;
    let listing = serde_json::to_value(fresh.listing()).context(|| "listing the versions".into())?;
    *l.manifest.lock().unwrap() = Some(fresh);
    Ok(listing)
}
#[tauri::command]
fn cached_manifest(l: L) -> Result<Option<serde_json::Value>> {
    let manifest = l.manifest.lock().unwrap();
    manifest.as_ref().map(|m| serde_json::to_value(m.listing()).context(|| "listing the versions".into())).transpose()
}

#[tauri::command]
async fn installed(l: L<'_>) -> Result<Vec<install::Installed>> {
    let home = l.home.clone();
    tauri::async_runtime::spawn_blocking(move || install::installed(&home)).await.context(|| "listing versions".into())?
}
#[tauri::command]
fn install(l: L, version: String) -> Result<()> {
    let release = match l.manifest.lock().unwrap().as_ref().and_then(|m| m.release(&version)) {
        Some(release) => release.clone(),
        None => return refused(format!("{version} isn't in the list of versions. Check for versions again.")),
    };
    release.runnable()?;
    l.installs.start(l.app.clone(), l.home.clone(), l.http.clone(), release)
}
#[tauri::command]
fn cancel(l: L, version: String) -> Result<()> {
    l.installs.cancel(&version)
}
#[tauri::command]
fn remove(l: L, version: String) -> Result<()> {
    if l.runner.running().as_deref() == Some(version.as_str()) {
        return refused(format!("{version} is running. Close the game first."));
    }
    if l.installs.busy(&version) {
        return refused(format!("{version} is installing. Cancel it first."));
    }
    install::remove(&l.home, &version)
}

#[tauri::command]
fn game(l: L) -> events::Game {
    l.runner.get()
}
// async: tokio::process spawns the game, which needs Tokio's runtime; a sync
// command runs on the GTK main thread, outside it, and panics there.
#[tauri::command]
async fn launch(l: L<'_>, version: String) -> Result<()> {
    let settings = l.settings.lock().unwrap().clone();
    let launcher = Arc::clone(&l);
    l.runner.launch(&l.app, &l.home, &version, move |app| {
        // the game may have signed in or out while it ran
        launcher.account.reload();
        if let Some(window) = app.get_webview_window("main") {
            let _ = window.show();
            let _ = window.set_focus();
        }
    })?;
    if !settings.keep_old {
        prune(&l, &version, settings.pinned.as_deref());
    }
    let window = l.app.get_webview_window("main");
    match (settings.on_launch, window) {
        (OnLaunch::Hide, Some(window)) => window.hide().context(|| "hiding the launcher".into())?,
        (OnLaunch::Close, _) => l.app.exit(0),
        _ => {}
    }
    Ok(())
}

/// Keep old versions off: those older than the one just played go, except a pinned one.
fn prune(l: &Launcher, played: &str, pinned: Option<&str>) {
    let Ok(installed) = install::installed(&l.home) else { return };
    for old in installed.iter().map(|i| i.id()) {
        if version::compare(old, played).is_lt() && Some(old) != pinned {
            if let Err(e) = install::remove(&l.home, old) {
                eprintln!("launcher: couldn't remove {old}: {e}");
            }
        }
    }
}

#[tauri::command]
fn collect_report(l: L, session: Option<String>) -> Result<report::Report> {
    report::collect(&l.home, session.as_deref())
}
#[tauri::command]
async fn submit_report(l: L<'_>, session: String, note: String) -> Result<()> {
    report::submit(&l.http, l.account.api(), l.account.key(), &l.home, &session, &note).await?;
    l.runner.dismiss(&l.app);
    Ok(())
}
#[tauri::command]
fn dismiss_crash(l: L) {
    l.runner.dismiss(&l.app);
}

#[tauri::command]
async fn launcher_update(l: L<'_>) -> Result<Option<update::Available>> {
    l.updates.check(&l.app).await
}
#[tauri::command]
async fn update_launcher(l: L<'_>) -> Result<()> {
    l.updates.install(&l.app, l.runner.running().is_some()).await
}

#[tauri::command]
fn open_data_folder(l: L) -> Result<()> {
    l.app.opener().open_path(l.home.root().display().to_string(), None::<&str>).context(|| "opening the data folder".into())
}
#[tauri::command]
fn open_url(l: L, url: String) -> Result<()> {
    if !url.starts_with("https://") {
        return refused(format!("Not a web address: {url}"));
    }
    l.app.opener().open_url(url, None::<&str>).context(|| "opening the browser".into())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        // a second launch brings the first forward instead
        .plugin(tauri_plugin_single_instance::init(|app, _, _| {
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.show();
                let _ = window.set_focus();
            }
        }))
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .setup(|app| {
            let launcher = Launcher::new(app.handle()).map_err(|e| e.to_string())?;
            app.manage(Arc::new(launcher));
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            about, account, sign_in, cancel_sign_in, retry_sign_in, sign_out,
            social, request_friend, accept_friend, remove_friend,
            settings, set_settings, refresh_manifest, cached_manifest,
            installed, install, cancel, remove,
            game, launch, collect_report, submit_report, dismiss_crash,
            launcher_update, update_launcher, open_data_folder, open_url,
        ])
        .run(tauri::generate_context!())
        .expect("the launcher failed to start");
}
