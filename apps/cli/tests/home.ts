// Preloaded before every test file (bunfig.toml): a throwaway home, so nothing a test runs
// (terra init, a sign-in, moving in) can touch the person's real ~/.terraverse[-dev].
import { mkdtempSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"

process.env.TERRAVERSE_HOME = mkdtempSync(join(tmpdir(), "terra-test-home-"))
delete process.env.TERRAVERSE_PROFILE
