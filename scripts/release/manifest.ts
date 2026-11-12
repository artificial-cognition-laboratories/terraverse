// Writes a release's manifest.json: what the game and install.sh read to find a
// version's builds and check them. CI signs the result (.github/workflows/release.yml),
// once, after every platform has built: one manifest names them all.
// Usage: bun scripts/release/manifest.ts <tag> <out> <platform>=<tarball>...
import { createHash } from "node:crypto"
import { basename } from "node:path"

// The oldest launcher (apps/launcher) that can install and run this build:
// raise it when a build needs something only a newer launcher does.
const LAUNCHER = "0.1.0"

const [tag, out, ...pairs] = process.argv.slice(2)
if (!tag || !out || pairs.length === 0)
    throw new Error("usage: manifest.ts <tag> <out> <platform>=<tarball>...")
const repo = process.env.GITHUB_REPOSITORY
if (!repo) throw new Error("manifest: GITHUB_REPOSITORY is not set")

// Who can play with whom: the launcher shows it beside each version.
const source = await Bun.file(new URL("../../apps/game/src/network/networkprotocol.cpp", import.meta.url)).text()
const protocol = source.match(/const u16 LATEST_PROTOCOL_VERSION = (\d+);/)?.[1]
if (!protocol) throw new Error("manifest: LATEST_PROTOCOL_VERSION isn't in networkprotocol.cpp any more")

const builds: Record<string, { url: string; sha256: string; size: number }> = {}
for (const pair of pairs) {
    const [platform, tarball] = pair.split("=")
    if (!platform || !tarball) throw new Error(`manifest: "${pair}" isn't <platform>=<tarball>`)
    const bytes = new Uint8Array(await Bun.file(tarball).arrayBuffer())
    builds[platform] = {
        url: `https://github.com/${repo}/releases/download/${tag}/${basename(tarball)}`,
        sha256: createHash("sha256").update(bytes).digest("hex"),
        size: bytes.length,
    }
}
const manifest = { version: tag.replace(/^v/, ""), protocol: Number(protocol), launcher: LAUNCHER, builds }
await Bun.write(out, `${JSON.stringify(manifest, null, 2)}\n`)
console.log(`manifest: ${out} for ${tag}, ${Object.keys(builds).join(", ")}`)
