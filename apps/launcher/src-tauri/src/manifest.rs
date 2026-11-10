// The versions there are: every GitHub release that carries a manifest signed
// by CI (scripts/release/manifest.ts), checked against the key the game trusts.
// A pre-release is a beta; the release's text is its notes. The last list is
// kept, so the launcher still knows what it has when offline.

use crate::failure::{refused, Context, Failure, Result};
use crate::{home, settings, signature};
use futures_util::future::join_all;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::Path;

pub const REPO: &str = "artificial-cognition-laboratories/terraverse";

#[cfg(target_os = "linux")]
pub const PLATFORM: &str = "linux-x86_64";
#[cfg(windows)]
pub const PLATFORM: &str = "windows-x86_64";

/// One version, as the page sees it (src/backend/types.ts: Version).
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Version {
    pub id: String,
    pub channel: Channel,
    pub released: String,
    pub notes: String,
    pub size: u64,
    pub protocol: Option<u32>, // in manifests from 0.1.3
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum Channel {
    Stable,
    Beta,
}

/// Where a version's build is, and what it must hash to: never sent to the page.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Build {
    pub url: String,
    pub sha256: String,
    pub size: u64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Release {
    pub version: Version,
    pub build: Build,
    /// The oldest launcher that can run it (none in manifests before 0.1.3).
    pub launcher: Option<String>,
}

impl Release {
    /// Refuses a build this launcher is too old to run.
    pub fn runnable(&self) -> Result<()> {
        match &self.launcher {
            Some(needed) if crate::version::compare(env!("CARGO_PKG_VERSION"), needed).is_lt() => refused(format!(
                "{} needs launcher {needed} or newer; this is {}. Update the launcher first.",
                self.version.id,
                env!("CARGO_PKG_VERSION")
            )),
            _ => Ok(()),
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Manifest {
    pub releases: Vec<Release>, // newest first
    pub fetched: u64,           // ms since the epoch
}

/// What the page is sent: the versions, without where their builds are.
#[derive(Serialize)]
pub struct Listing<'a> {
    versions: Vec<&'a Version>,
    fetched: u64,
}

impl Manifest {
    pub fn listing(&self) -> Listing<'_> {
        Listing { versions: self.releases.iter().map(|r| &r.version).collect(), fetched: self.fetched }
    }
    pub fn release(&self, id: &str) -> Option<&Release> {
        self.releases.iter().find(|r| r.version.id == id)
    }
    pub fn load(path: &Path) -> Result<Option<Manifest>> {
        if !path.exists() {
            return Ok(None);
        }
        let text = std::fs::read_to_string(path).context(|| format!("reading {}", path.display()))?;
        serde_json::from_str(&text).context(|| format!("{} is damaged", path.display())).map(Some)
    }
    pub fn save(&self, path: &Path) -> Result<()> {
        let text = serde_json::to_vec_pretty(self).context(|| "writing the versions".into())?;
        settings::write_atomically(path, &text)
    }
}

// GitHub's answer, as much of it as we read
#[derive(Deserialize)]
struct GitRelease {
    tag_name: String,
    draft: bool,
    prerelease: bool,
    published_at: Option<String>,
    body: Option<String>,
    assets: Vec<Asset>,
}
#[derive(Deserialize)]
struct Asset {
    name: String,
    browser_download_url: String,
}

// a release's manifest.json
#[derive(Deserialize)]
struct Signed {
    version: String,
    builds: HashMap<String, Build>,
    protocol: Option<u32>,
    launcher: Option<String>,
}

pub async fn fetch(http: &reqwest::Client) -> Result<Manifest> {
    let answer = http
        .get(format!("https://api.github.com/repos/{REPO}/releases?per_page=50"))
        .header("Accept", "application/vnd.github+json")
        .send()
        .await;
    let answer = match answer {
        Ok(answer) => answer,
        Err(_) => return refused("Couldn't reach GitHub to see what versions there are."),
    };
    match answer.status().as_u16() {
        200 => {}
        403 | 429 => return refused("GitHub is limiting how often we can ask. Try again in a few minutes."),
        code => return refused(format!("GitHub answered {code} when asked for the versions.")),
    }
    let releases: Vec<GitRelease> = answer.json().await.context(|| "reading GitHub's list of releases".into())?;

    let checked = join_all(releases.into_iter().filter(|r| !r.draft).map(|r| check(http, r))).await;
    let mut out = Vec::new();
    for release in checked {
        match release {
            Ok(Some(release)) => out.push(release),
            Ok(None) => {}
            // one bad release mustn't hide the rest; it just isn't offered
            Err(e) => eprintln!("launcher: skipping a release: {e}"),
        }
    }
    out.sort_by(|a, b| crate::version::compare(&b.version.id, &a.version.id));
    Ok(Manifest { releases: out, fetched: now_ms() })
}

/// A release we can offer: signed, and with a build for this platform. None if
/// it has no manifest (from before they existed) or nothing for us.
async fn check(http: &reqwest::Client, release: GitRelease) -> Result<Option<Release>> {
    let asset = |name: &str| release.assets.iter().find(|a| a.name == name).map(|a| a.browser_download_url.clone());
    let (Some(manifest_url), Some(signature_url)) = (asset("manifest.json"), asset("manifest.json.sig")) else {
        return Ok(None);
    };
    let bytes = download(http, &manifest_url).await?;
    let sig = download(http, &signature_url).await?;
    if !signature::verify(&bytes, &sig, signature::UPDATE_KEY) {
        return Err(Failure::Bug(format!("{}: its manifest's signature is wrong", release.tag_name)));
    }
    let signed: Signed = serde_json::from_slice(&bytes).context(|| format!("{}: reading its manifest", release.tag_name))?;
    if signed.version != release.tag_name.trim_start_matches('v') {
        return Err(Failure::Bug(format!("{}: its manifest is for {}", release.tag_name, signed.version)));
    }
    home::checked(&signed.version)?;
    let Some(build) = signed.builds.get(PLATFORM).cloned() else {
        return Ok(None);
    };
    Ok(Some(Release {
        version: Version {
            id: signed.version,
            channel: if release.prerelease { Channel::Beta } else { Channel::Stable },
            released: release.published_at.as_deref().map_or("", |d| d.get(..10).unwrap_or(d)).to_owned(),
            notes: release.body.unwrap_or_default(),
            size: build.size,
            protocol: signed.protocol,
        },
        build,
        launcher: signed.launcher,
    }))
}

async fn download(http: &reqwest::Client, url: &str) -> Result<Vec<u8>> {
    let answer = http.get(url).send().await.and_then(|a| a.error_for_status());
    match answer {
        Ok(answer) => Ok(answer.bytes().await.context(|| format!("downloading {url}"))?.to_vec()),
        Err(e) => refused(format!("Couldn't download {url}: {e}")),
    }
}

pub fn now_ms() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_millis() as u64)
}

#[cfg(test)]
mod tests {
    use super::*;

    // On the network, against the real releases: `cargo test -- --ignored`.
    #[tokio::test]
    #[ignore]
    async fn reads_the_real_releases() {
        let http = reqwest::Client::builder().user_agent("terraverse-launcher-test").build().unwrap();
        let manifest = fetch(&http).await.unwrap();
        assert!(!manifest.releases.is_empty(), "no signed release for {PLATFORM}");
        for release in &manifest.releases {
            println!("{} {:?} {} {} bytes", release.version.id, release.version.channel, release.version.released, release.build.size);
            assert!(release.build.url.starts_with("https://github.com/"));
            assert_eq!(release.build.sha256.len(), 64);
        }
    }
}
