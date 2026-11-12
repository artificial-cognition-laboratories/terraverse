// Releasing the launcher, apart from the game and more rarely: `arc deploy launcher`
// (repo.config.ts). Its version is tauri.conf.json's; a launcher-v<version> tag
// builds and publishes it (.github/workflows/launcher.yml).
//
//   bun release.ts gate   the versions agree, the UI typechecks, the backend's tests pass
//   bun release.ts tag    push, then tag launcher-v<version>
//   bun release.ts wait   until the release and the rolling updater point at it
import { run } from "../../scripts/release/version"

const here = import.meta.dir
const BUDGET_MS = 28 * 60_000 // under arc's 30-minute step limit
const LOOK_MS = 30_000

function version(): string {
    const conf: unknown = JSON.parse(require("fs").readFileSync(`${here}/src-tauri/tauri.conf.json`, "utf8"))
    const v = typeof conf === "object" && conf !== null && "version" in conf ? conf.version : undefined
    if (typeof v !== "string" || !/^\d+\.\d+\.\d+$/.test(v))
        throw new Error(`launcher: tauri.conf.json needs a "version" like 0.2.0 (has ${JSON.stringify(v)})`)
    return v
}

async function gate() {
    const v = version()
    const cargo = (await Bun.file(`${here}/src-tauri/Cargo.toml`).text()).match(/^version = "(.+)"$/m)?.[1]
    if (cargo !== v) throw new Error(`launcher: Cargo.toml says ${cargo}, tauri.conf.json says ${v}`)
    console.log(await run(["bun", "run", "typecheck"]))
    const proc = Bun.spawn(["cargo", "test"], { cwd: `${here}/src-tauri`, stdout: "inherit", stderr: "inherit" })
    if ((await proc.exited) !== 0) throw new Error("launcher: cargo test failed")
    console.log(`launcher: ${v} is ready to tag`)
}

async function tag() {
    const t = `launcher-v${version()}`
    if ((await run(["git", "tag", "--list", t])) !== "")
        throw new Error(`launcher: ${t} is already tagged here — bump "version" in tauri.conf.json and Cargo.toml`)
    if ((await run(["git", "ls-remote", "--tags", "origin", `refs/tags/${t}`])) !== "")
        throw new Error(`launcher: ${t} already exists on GitHub — bump "version" in tauri.conf.json and Cargo.toml`)
    await run(["git", "push", "origin", "HEAD"])
    await run(["git", "tag", "-a", t, "-m", `Terraverse launcher ${t}`])
    await run(["git", "push", "origin", t])
    console.log(`launcher: pushed ${t}; GitHub Actions is building it`)
}

// Out when its own release has every build and the rolling release's
// latest.json names it: only then do installed launchers update to it.
async function wait() {
    const v = version()
    const t = `launcher-v${v}`
    const assets = [`terraverse-launcher-${v}-x86_64.AppImage`, `terraverse-launcher_${v}_amd64.deb`, `terraverse-launcher-${v}-x64-setup.exe`]
    const started = Date.now()
    for (;;) {
        const runs = JSON.parse(await run(["gh", "run", "list", "--workflow", "launcher.yml", "--branch", t,
            "--json", "status,conclusion,url", "--limit", "1"])) as { status: string; conclusion: string; url: string }[]
        const latest = runs[0]
        if (latest?.status === "completed" && latest.conclusion !== "success")
            throw new Error(`launcher: the ${t} build ${latest.conclusion}: ${latest.url}`)

        const release = await run(["gh", "release", "view", t, "--json", "assets,url"]).catch(() => null)
        const rolling = await run(["gh", "release", "download", "launcher", "-p", "latest.json", "-O", "-"]).catch(() => null)
        if (release && rolling) {
            const { assets: attached, url } = JSON.parse(release) as { assets: { name: string }[]; url: string }
            const missing = assets.filter(name => !attached.some(asset => asset.name === name))
            const pointed = (JSON.parse(rolling) as { version: string }).version === v
            if (missing.length === 0 && pointed) {
                console.log(`launcher: ${t} is out, and installed launchers will update to it: ${url}`)
                process.exit(0)
            }
        }
        if (Date.now() - started > BUDGET_MS)
            throw new Error(`launcher: ${t} is still building${latest ? ` (${latest.url})` : ""}; run \`bun release.ts wait\` to keep waiting`)
        console.log(`launcher: waiting for ${t}${latest ? ` (${latest.status})` : ""}…`)
        await Bun.sleep(LOOK_MS)
    }
}

const steps: Record<string, () => Promise<void>> = { gate, tag, wait }
const step = steps[process.argv[2] ?? ""]
if (!step) throw new Error(`usage: bun release.ts ${Object.keys(steps).join("|")}`)
await step()
