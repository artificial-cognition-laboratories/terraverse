// Writes a release's manifest.json: what the game and install.sh read to find a
// version's build and check it. CI signs the result (.github/workflows/release.yml).
// Usage: bun scripts/release/manifest.ts <tag> <platform> <tarball> <out>
import { createHash } from "node:crypto"
import { basename } from "node:path"

const [tag, platform, tarball, out] = process.argv.slice(2)
if (!tag || !platform || !tarball || !out)
    throw new Error("usage: manifest.ts <tag> <platform> <tarball> <out>")
const repo = process.env.GITHUB_REPOSITORY
if (!repo) throw new Error("manifest: GITHUB_REPOSITORY is not set")

const bytes = new Uint8Array(await Bun.file(tarball).arrayBuffer())
const manifest = {
    version: tag.replace(/^v/, ""),
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
