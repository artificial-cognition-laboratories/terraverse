// The `terra` and `terraverse` commands in ~/.local/bin (Linux): alone, the launcher; with a
// command, the terra CLI the launcher links for the version last played (game.rs: point_cli).
// install.sh writes them once; the launcher keeps them current on every start, so a command
// added after someone installed (terra came after terraverse) reaches them with an update.

use std::path::{Path, PathBuf};

use crate::failure::{Context, Result};
use crate::home::Home;

/// What's in a shim of ours, and in every one install.sh ever wrote: never touch another file.
const OURS: &str = "Terraverse.AppImage";
const NAMES: [&str; 2] = ["terra", "terraverse"];

/// Writes both shims, when the launcher runs as an AppImage and ~/.local/bin exists. A file
/// there that isn't ours is left alone.
pub fn refresh(home: &Home) -> Result<()> {
    let (Some(appimage), Some(user)) = (std::env::var_os("APPIMAGE"), std::env::var_os("HOME")) else {
        return Ok(()); // a dev build, or not Linux: nothing to point at
    };
    let bin = PathBuf::from(user).join(".local").join("bin");
    if !bin.is_dir() {
        return Ok(());
    }
    // no FUSE 2: install.sh found it so, and the launcher was started this way since
    let extract = std::env::var_os("APPIMAGE_EXTRACT_AND_RUN").is_some();
    let run = command(Path::new(&appimage), extract);
    for name in NAMES {
        let path = bin.join(name);
        if !replaceable(&path)? {
            continue;
        }
        write(&path, &shim(name, &run, &home.cli()))?;
    }
    Ok(())
}

fn command(appimage: &Path, extract: bool) -> String {
    let app = quoted(&appimage.to_string_lossy());
    if extract { format!("env APPIMAGE_EXTRACT_AND_RUN=1 {app}") } else { app }
}

/// The shim, as install.sh writes it.
fn shim(name: &str, run: &str, cli: &Path) -> String {
    format!(
        "#!/bin/sh\n\
         [ $# -eq 0 ] && exec {run}\n\
         cli={cli}\n\
         [ -x \"$cli\" ] || {{ echo \"{name}: play a version from the launcher once first: its terra comes with it\" >&2; exit 1; }}\n\
         exec \"$cli\" \"$@\"\n",
        cli = quoted(&cli.to_string_lossy()),
    )
}

/// Missing, or a shim of ours.
fn replaceable(path: &Path) -> Result<bool> {
    match std::fs::read(path) {
        Ok(bytes) => Ok(String::from_utf8_lossy(&bytes).contains(OURS)),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(std::fs::symlink_metadata(path).is_err()),
        Err(e) => Err(e).context(|| format!("reading {}", path.display())),
    }
}

/// Unchanged files aren't rewritten; a changed one is swapped in whole.
fn write(path: &Path, body: &str) -> Result<()> {
    if std::fs::read_to_string(path).is_ok_and(|now| now == body) {
        return Ok(());
    }
    let tmp = path.with_extension("new");
    std::fs::write(&tmp, body).context(|| format!("writing {}", tmp.display()))?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&tmp, std::fs::Permissions::from_mode(0o755))
            .context(|| format!("making {} runnable", tmp.display()))?;
    }
    std::fs::rename(&tmp, path).context(|| format!("replacing {}", path.display()))
}

fn quoted(s: &str) -> String {
    format!("'{}'", s.replace('\'', "'\\''"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn leaves_other_files_alone() {
        let dir = tempfile::tempdir().unwrap();
        let theirs = dir.path().join("terra");
        std::fs::write(&theirs, "#!/bin/sh\necho mine\n").unwrap();
        assert!(!replaceable(&theirs).unwrap());
        let old = dir.path().join("terraverse");
        std::fs::write(&old, "#!/bin/sh\nexec /home/x/.terraverse/launcher/Terraverse.AppImage \"$@\"\n").unwrap();
        assert!(replaceable(&old).unwrap());
        assert!(replaceable(&dir.path().join("missing")).unwrap());
    }

    #[test]
    fn writes_a_shim_that_finds_the_cli() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("terra");
        let body = shim("terra", &command(Path::new("/h/.terraverse/launcher/Terraverse.AppImage"), false), Path::new("/h/.terraverse/bin/terra"));
        write(&path, &body).unwrap();
        let read = std::fs::read_to_string(&path).unwrap();
        assert!(read.contains("exec '/h/.terraverse/launcher/Terraverse.AppImage'"));
        assert!(read.contains("cli='/h/.terraverse/bin/terra'"));
        assert!(replaceable(&path).unwrap());
    }
}
