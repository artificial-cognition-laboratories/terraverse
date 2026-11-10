// Writes a release's manifest.json: what the game and install.sh read to find a
// version's build and check it. CI signs the result (.github/workflows/release.yml).
// Usage: bun scripts/release/manifest.ts <tag> <platform> <tarball> <out>
import { createHash } from "node:crypto"
import { basename } from "node:path"

// The oldest launcher (apps/launcher) that can install and run this build:
// raise it when a build needs something only a newer launcher does.
const LAUNCHER = "0.1.0"

const [tag, platform, tarball, out] = process.argv.slice(2)
if (!tag || !platform || !tarball || !out)
    throw new Error("usage: manifest.ts <tag> <platform> <tarball> <out>")
const repo = process.env.GITHUB_REPOSITORY
if (!repo) throw new Error("manifest: GITHUB_REPOSITORY is not set")

// Who can play with whom: the launcher shows it beside each version.
const source = await Bun.file(new URL("../../apps/game/src/network/networkprotocol.cpp", import.meta.url)).text()
const protocol = source.match(/const u16 LATEST_PROTOCOL_VERSION = (\d+);/)?.[1]
if (!protocol) throw new Error("manifest: LATEST_PROTOCOL_VERSION isn't in networkprotocol.cpp any more")

const bytes = new Uint8Array(await Bun.file(tarball).arrayBuffer())
const manifest = {
    version: tag.replace(/^v/, ""),
    protocol: Number(protocol),
    launcher: LAUNCHER,
    builds: {
        [platform]: {
            url: `https://github.com/${repo}/releases/download/${tag}/${basename(tarball)}`,
            sha256: createHash("sha256").update(bytes).digest("hex"),
            size: bytes.length,
        },
    },
}
await Bun.write(out, `${JSON.stringify(manifest, null, 2)}\n`)
console.log(`manifest: ${out} for ${tag}, ${platform}`)
