## Bug : terraverse 1.3 

my cursor doesnt show above the game. it still interacts but it doesnt draw over the game. 

## Outcome

Fixed in the launcher (apps/launcher/src-tauri/src/game.rs, commit "Launcher: the game no longer inherits the AppImage's environment").

Cause: 0.1.3 is the first version run from the launcher, an AppImage. Its AppRun puts the AppImage's
own libraries on LD_LIBRARY_PATH (Ubuntu 22.04's libwayland-cursor.so.0, libXcursor.so.1, GTK and
friends) and its GTK hook points GTK/GIO/XDG_DATA_DIRS into the mount. The game inherited all of it
(seen in the running launcher's /proc environ). The game package bundles its own libraries but leaves
libwayland-*/libX* to the desktop, so those resolved to the AppImage's old copies: input fine, cursor
not drawn. No engine change between v0.1.2 and v0.1.3 touches the cursor.

Fix: when the launcher runs from an AppImage (APPDIR set), the game's environment drops path entries
inside the mount and the variables AppRun and the GTK hook set (APPDIR, APPIMAGE, ARGV0, OWD,
GTK_THEME, WEBKIT_DISABLE_DMABUF_RENDERER). Unit test `leaves_the_appimage_behind`.

Verified: `cargo test` in the launcher passes. Running the dev engine with the launcher's
LD_LIBRARY_PATH fails outright (old libnghttp2 from the mount), which confirms the leak loads the
AppImage's libraries. Not verified: the cursor itself in a launcher-run 0.1.3 (no version installed
here, and grim doesn't capture this compositor's cursor). Ships with the next launcher release, not
the game's. Call made: GTK_THEME is dropped even if the user set it themselves (the hook overwrites
it anyway, and the game doesn't use GTK).
