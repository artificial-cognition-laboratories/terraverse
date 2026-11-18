# terra prepare fails on Windows when the server folder is reached by an 8.3 short path

Found by the CI owner, 2026-11-18, from terra's tests on the Windows runner (run 44397404612),
where TEMP is `C:\Users\RUNNER~1\AppData\Local\Temp`. A server folder reached by that short path
fails `bun install` on its local mods:

    INSTALL_FAILED: error: Could not find package.json for
      "file:../../../../../runneradmin/AppData/Local/Temp/terra-server-X/mods/hello" dependency "@check/hello"
    error: @check/hello@file:../hello failed to resolve

bun resolves the `file:` dependencies against the long name (runneradmin) while the install runs in
the short one, and the relative path between them lands nowhere. The tests now make their fixture
under the real path (`realpathSync.native(tmpdir())`, apps/cli/tests/fixtures.ts), so CI is green; a
player whose folder is reached by a short path (a long user name or one with spaces, under %TEMP%, or
typed short) still hits it.

Likely fix: open projects by their real path (`realpathSync.native` in `projects.open`), so terra
and bun agree on where a project is. Reproduce on Windows: `terra prepare C:\Users\RUNNER~1\...`
(any `dir /x` short name of a folder holding a server with a local mod).
