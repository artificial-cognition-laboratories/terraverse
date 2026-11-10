// Installing a version: download its build (resuming a partial one), check it
// against its signed hash, unpack it beside the others and rename it into
// place. A version is complete or absent; nothing half-done is ever listed.

use crate::events::{self, Message};
use crate::failure::{refused, Context, Failure, Result};
use crate::home::Home;
use crate::manifest::Release;
use futures_util::StreamExt;
use serde::Serialize;
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

#[derive(Serialize)]
pub struct Installed {
    id: String,
    size: u64,
}

impl Installed {
    pub fn id(&self) -> &str {
        &self.id
    }
}

/// The downloads under way, each with its cancel flag.
#[derive(Default)]
pub struct Installs {
    running: Mutex<HashMap<String, Arc<AtomicBool>>>,
}

impl Installs {
    pub fn busy(&self, id: &str) -> bool {
        self.running.lock().unwrap().contains_key(id)
    }

    pub fn start(self: &Arc<Self>, app: tauri::AppHandle, home: Arc<Home>, http: reqwest::Client, release: Release) -> Result<()> {
        let id = release.version.id.clone();
        let target = home.version(&id)?;
        if target.exists() {
            return refused(format!("{id} is already installed."));
        }
        let cancel = Arc::new(AtomicBool::new(false));
        {
            let mut running = self.running.lock().unwrap();
            if running.contains_key(&id) {
                return refused(format!("{id} is already installing."));
            }
            running.insert(id.clone(), cancel.clone());
        }
        let installs = self.clone();
        tauri::async_runtime::spawn(async move {
            let outcome = install(&app, &home, &http, &release, &cancel).await;
            installs.running.lock().unwrap().remove(&id);
            match outcome {
                Ok(true) => events::send(&app, Message::Installed { version: id }),
                Ok(false) => {} // cancelled: the page already knows
                Err(e) => {
                    eprintln!("launcher: installing {id}: {e}");
                    events::send(&app, Message::InstallFailed { version: id, reason: e.words().to_owned() });
                }
            }
        });
        Ok(())
    }

    pub fn cancel(&self, id: &str) -> Result<()> {
        match self.running.lock().unwrap().get(id) {
            Some(flag) => Ok(flag.store(true, Ordering::Relaxed)),
            None => refused(format!("{id} isn't downloading.")),
        }
    }
}

/// True when installed, false when cancelled.
async fn install(app: &tauri::AppHandle, home: &Home, http: &reqwest::Client, release: &Release, cancel: &AtomicBool) -> Result<bool> {
    let id = &release.version.id;
    let staging = home.staging();
    std::fs::create_dir_all(&staging).context(|| format!("creating {}", staging.display()))?;
    let archive = staging.join(format!("{id}.download"));

    if !download(app, http, release, &archive, cancel).await? {
        return Ok(false);
    }
    let sum = {
        let archive = archive.clone();
        tauri::async_runtime::spawn_blocking(move || sha256(&archive)).await.context(|| "hashing the download".into())??
    };
    if !sum.eq_ignore_ascii_case(&release.build.sha256) {
        std::fs::remove_file(&archive).context(|| format!("removing {}", archive.display()))?;
        return refused("The download didn't match what the release says it should be, so it wasn't installed. Try again.");
    }

    let unpacked = staging.join(format!("{id}.unpack"));
    let target = home.version(id)?;
    let from = archive.clone();
    tauri::async_runtime::spawn_blocking(move || unpack(&from, &unpacked, &target))
        .await
        .context(|| "unpacking".into())??;
    std::fs::remove_file(&archive).context(|| format!("removing {}", archive.display()))?;
    Ok(true)
}

/// Streams the build to `to`, carrying on from what's there. False if cancelled.
async fn download(app: &tauri::AppHandle, http: &reqwest::Client, release: &Release, to: &Path, cancel: &AtomicBool) -> Result<bool> {
    let id = &release.version.id;
    let total = release.build.size;
    let mut have = std::fs::metadata(to).map_or(0, |m| m.len());
    if have >= total {
        return Ok(true); // whole already; the hash decides if it's right
    }
    let mut request = http.get(&release.build.url);
    if have > 0 {
        request = request.header("Range", format!("bytes={have}-"));
    }
    let answer = match request.send().await.and_then(|a| a.error_for_status()) {
        Ok(answer) => answer,
        Err(_) => return refused("Couldn't reach GitHub to download it. Check your connection and try again."),
    };
    // 206 carries on; 200 means the server sent it all again
    let resumed = answer.status().as_u16() == 206;
    if !resumed {
        have = 0;
    }
    let mut file = std::fs::OpenOptions::new()
        .create(true)
        .write(true)
        .append(resumed)
        .truncate(!resumed)
        .open(to)
        .context(|| format!("opening {}", to.display()))?;

    let mut stream = answer.bytes_stream();
    let mut told = Instant::now() - Duration::from_secs(1);
    while let Some(chunk) = stream.next().await {
        if cancel.load(Ordering::Relaxed) {
            return Ok(false); // what's there is kept, to carry on from
        }
        let chunk = match chunk {
            Ok(chunk) => chunk,
            Err(_) => return refused("The download stopped part way. Try again to carry on from there."),
        };
        file.write_all(&chunk).context(|| format!("writing {}", to.display()))?;
        have += chunk.len() as u64;
        if told.elapsed() >= Duration::from_millis(100) {
            told = Instant::now();
            events::send(app, Message::Install { version: id.clone(), received: have.min(total), total });
        }
    }
    events::send(app, Message::Install { version: id.clone(), received: have.min(total), total });
    Ok(true)
}

fn sha256(path: &Path) -> Result<String> {
    let mut file = std::fs::File::open(path).context(|| format!("opening {}", path.display()))?;
    let mut hash = Sha256::new();
    let mut buffer = vec![0; 1 << 20];
    loop {
        let n = file.read(&mut buffer).context(|| format!("reading {}", path.display()))?;
        if n == 0 {
            break;
        }
        hash.update(&buffer[..n]);
    }
    Ok(hex::encode(hash.finalize()))
}

/// A .tar.zst holding one folder (scripts/release/package-linux.sh): that
/// folder becomes the version. tar refuses entries that would land outside.
fn unpack(archive: &Path, scratch: &Path, target: &Path) -> Result<()> {
    if scratch.exists() {
        std::fs::remove_dir_all(scratch).context(|| format!("clearing {}", scratch.display()))?;
    }
    std::fs::create_dir_all(scratch).context(|| format!("creating {}", scratch.display()))?;
    let file = std::fs::File::open(archive).context(|| format!("opening {}", archive.display()))?;
    let decoder = zstd::Decoder::new(file).context(|| "reading the download".into())?;
    let mut tar = tar::Archive::new(decoder);
    tar.set_preserve_permissions(true);
    tar.unpack(scratch).map_err(|e| Failure::Refused(format!("The download couldn't be unpacked ({e}). Try again.")))?;

    let mut entries: Vec<PathBuf> = std::fs::read_dir(scratch)
        .context(|| format!("reading {}", scratch.display()))?
        .map(|e| e.map(|e| e.path()))
        .collect::<std::io::Result<_>>()
        .context(|| format!("reading {}", scratch.display()))?;
    let root = match (entries.pop(), entries.is_empty()) {
        (Some(only), true) if only.is_dir() => only,
        _ => return Err(Failure::Bug("the build isn't one folder; scripts/release/package-linux.sh makes one".into())),
    };
    std::fs::rename(&root, target).context(|| format!("moving the build to {}", target.display()))?;
    std::fs::remove_dir_all(scratch).context(|| format!("clearing {}", scratch.display()))
}

pub fn installed(home: &Home) -> Result<Vec<Installed>> {
    let dir = home.versions();
    if !dir.exists() {
        return Ok(Vec::new());
    }
    let mut out = Vec::new();
    for entry in std::fs::read_dir(&dir).context(|| format!("reading {}", dir.display()))? {
        let entry = entry.context(|| format!("reading {}", dir.display()))?;
        let name = entry.file_name().to_string_lossy().into_owned();
        if name.starts_with('.') || !entry.path().is_dir() {
            continue;
        }
        out.push(Installed { size: size(&entry.path()), id: name });
    }
    out.sort_by(|a, b| crate::version::compare(&b.id, &a.id));
    Ok(out)
}

pub fn remove(home: &Home, id: &str) -> Result<()> {
    let dir = home.version(id)?;
    if !dir.exists() {
        return refused(format!("{id} isn't installed."));
    }
    std::fs::remove_dir_all(&dir).context(|| format!("removing {}", dir.display()))
}

fn size(path: &Path) -> u64 {
    let Ok(entries) = std::fs::read_dir(path) else { return 0 };
    entries
        .flatten()
        .map(|e| match e.metadata() {
            Ok(m) if m.is_dir() => size(&e.path()),
            Ok(m) => m.len(),
            Err(_) => 0,
        })
        .sum()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pack(dir: &Path, root: &str) -> PathBuf {
        let archive = dir.join("build.tar.zst");
        let file = std::fs::File::create(&archive).unwrap();
        let encoder = zstd::Encoder::new(file, 3).unwrap().auto_finish();
        let mut tar = tar::Builder::new(encoder);
        let mut header = tar::Header::new_gnu();
        let body = b"#!/bin/sh\n";
        header.set_size(body.len() as u64);
        header.set_mode(0o755);
        header.set_cksum();
        tar.append_data(&mut header, format!("{root}/terraverse"), &body[..]).unwrap();
        tar.finish().unwrap();
        drop(tar);
        archive
    }

    #[test]
    fn unpacks_the_one_folder_into_place() {
        let dir = tempfile::tempdir().unwrap();
        let archive = pack(dir.path(), "terraverse-v0.1.2-linux-x86_64");
        let target = dir.path().join("0.1.2");
        unpack(&archive, &dir.path().join("scratch"), &target).unwrap();
        assert!(target.join("terraverse").is_file());
        assert!(!dir.path().join("scratch").exists());
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            assert_eq!(std::fs::metadata(target.join("terraverse")).unwrap().permissions().mode() & 0o111, 0o111);
        }
    }

    #[test]
    fn hashes_files() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("x");
        std::fs::write(&path, b"abc").unwrap();
        assert_eq!(sha256(&path).unwrap(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }
}
