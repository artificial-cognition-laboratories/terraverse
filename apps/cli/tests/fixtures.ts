// What tests share: the check's server folder, copied somewhere they can change it.
import { cpSync, mkdtempSync, realpathSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { Terra } from "../src/terra"

export const SERVER_FIXTURE = `${import.meta.dir}/../../../scripts/mods-check/server`

/** A fresh copy of the check's server, and terra opened with this bun as its own. */
export function serverFixture() {
    // The real path, in the platform's separators: Windows runners' TEMP is an 8.3 short
    // name (RUNNER~1), which bun install resolves file: dependencies against wrongly.
    const root = mkdtempSync(join(realpathSync.native(tmpdir()), "terra-server-"))
    cpSync(SERVER_FIXTURE, root, { recursive: true })
    return { root: root, terra: Terra({ exec: process.execPath }) }
}
