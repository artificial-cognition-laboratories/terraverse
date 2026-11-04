// Waits until the release's builds are attached on GitHub, or its workflow
// fails. Safe to run again: if it gives up while CI is still building, run
// `bun run release:wait` to keep waiting.
import { releaseTag, run } from "./version"

const tag = releaseTag()
const ASSETS = [`terraverse-${tag}-linux-x86_64.tar.zst`, "manifest.json", "manifest.json.sig"]
const BUDGET_MS = 28 * 60_000 // under arc's 30-minute step limit
const LOOK_MS = 30_000

const started = Date.now()
for (;;) {
    const runs = JSON.parse(await run(["gh", "run", "list", "--workflow", "release.yml", "--branch", tag,
        "--json", "status,conclusion,url", "--limit", "1"])) as { status: string; conclusion: string; url: string }[]
    const latest = runs[0]
    if (latest?.status === "completed" && latest.conclusion !== "success")
        throw new Error(`release: the ${tag} build ${latest.conclusion}: ${latest.url}`)

    const release = await run(["gh", "release", "view", tag, "--json", "assets,url"]).catch(() => null)
    if (release) {
        const { assets, url } = JSON.parse(release) as { assets: { name: string }[]; url: string }
        const missing = ASSETS.filter(name => !assets.some(asset => asset.name === name))
        if (missing.length === 0) {
            console.log(`release: ${tag} is out: ${url}`)
            process.exit(0)
        }
    }
    if (Date.now() - started > BUDGET_MS)
        throw new Error(`release: ${tag} is still building${latest ? ` (${latest.url})` : ""}; run \`bun run release:wait\` to keep waiting`)
    console.log(`release: waiting for ${tag}${latest ? ` (${latest.status})` : ""}…`)
    await Bun.sleep(LOOK_MS)
}
