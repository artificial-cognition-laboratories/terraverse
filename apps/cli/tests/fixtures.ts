// What tests share: the check's server folder, copied somewhere they can change it.
import { cpSync, mkdtempSync } from "node:fs"
import { tmpdir } from "node:os"
import { Terra } from "../src/terra"

export const SERVER_FIXTURE = `${import.meta.dir}/../../../scripts/mods-check/server`

/** A fresh copy of the check's server, and terra opened with this bun as its own. */
export function serverFixture() {
    const root = mkdtempSync(`${tmpdir()}/terra-server-`)
    cpSync(SERVER_FIXTURE, root, { recursive: true })
    return { root: root, terra: Terra({ exec: process.execPath }) }
}
