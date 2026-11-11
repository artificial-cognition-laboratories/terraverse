// The dev loop: rebuild the engine, serve the UI with hot reload, and run the
// game against the dev server. Extra arguments go to luanti (e.g. --go).
// A second game alongside the first, as another player:
//     bun dev --profile two
// It shares the first one's UI server, and signs in on its own.

const root = `${import.meta.dir}/..`
const ui = `${root}/apps/game/ui`
const devUrl = "http://localhost:5173"
// Accounts and friends come from the local ArcLabs stack (`arc status` in
// ../arclabs lists it); ARCLABS_URL points elsewhere, e.g. production.
const arclabsUrl = process.env.ARCLABS_URL ?? "http://localhost:3099"

// Builds in this tree take turns on one lock (see scripts/build.sh).
const locked = (cmd: string[]) => ["flock", `${root}/.output/build.lock`, ...cmd]

async function run(cmd: string[], cwd: string, label: string) {
    const started = performance.now()
    const code = await Bun.spawn(cmd, { cwd: cwd, stdio: ["inherit", "inherit", "inherit"] }).exited
    if (code !== 0) throw new Error(`${cmd.join(" ")} exited with ${code}`)
    console.log(`dev: ${label} ${((performance.now() - started) / 1000).toFixed(2)}s`)
}

async function waitFor(url: string, timeoutMs: number) {
    const deadline = Date.now() + timeoutMs
    while (Date.now() < deadline) {
        const up = await fetch(url).then(() => true, () => false)
        if (up) return
        await Bun.sleep(200)
    }
    throw new Error(`UI dev server did not come up at ${url} within ${timeoutMs}ms`)
}

async function launchGame(args: string[]) {
    const deadline = Date.now() + 120_000
    const binary = `${root}/.output/bin/luanti`
    while (true) {
        try {
            return Bun.spawn(["./.output/bin/luanti", "--shell-url", devUrl, "--arclabs-url", arclabsUrl, ...args], {
                cwd: root,
                stdio: ["inherit", "inherit", "inherit"],
            })
        } catch (error) {
            // Another terminal may be linking the same executable. Linux
            // refuses to exec it until the writer closes the file.
            if (error instanceof Error && "code" in error) {
                const linking = error.code === "ETXTBSY" ||
                    (error.code === "EACCES" && Bun.file(binary).size === 0)
                if (linking) {
                    if (Date.now() >= deadline)
                        throw new Error(`Timed out waiting for ${binary} to finish linking`, { cause: error })
                    await Bun.sleep(250)
                    continue
                }
            }
            throw error
        }
    }
}

await Bun.$`mkdir -p ${root}/.output`
if (!(await Bun.file(`${root}/.output/build/CMakeCache.txt`).exists()))
    await run(locked(["cmake", "-S", ".", "-B", ".output/build", "-DRUN_IN_PLACE=TRUE"]), root, "configure")
// CMake regenerates its build files when CMake inputs change.
// (no link-time optimisation here: that's most of a rebuild's wait; a release has its own)
await run(locked(["cmake", "-S", ".", "-B", ".output/build", "-DRUN_IN_PLACE=TRUE", "-DENABLE_LTO=FALSE"]), root, "configure")
await run(["bash", "scripts/build.sh", "-j8"], root, "engine build")
await run(locked(["bash", "scripts/stage-runtime.sh"]), root, "runtime staging")
if (!(await Bun.file(`${ui}/node_modules/.bin/vite`).exists()))
    await run(["bun", "install"], ui, "UI dependencies")

// Another game's dev loop may already serve the UI: share it rather than fail
// on the port. (It goes away when that game closes.)
const serving = await fetch(devUrl).then(() => true, () => false)
const vite = serving ? null : Bun.spawn(["bun", "run", "dev"], { cwd: ui, stdio: ["ignore", "inherit", "inherit"] })
if (serving) console.log(`dev: sharing the UI server already at ${devUrl}`)
let game: ReturnType<typeof Bun.spawn> | null = null
const stop = () => {
    game?.kill("SIGINT") // lets the engine shut Chromium down cleanly
    vite?.kill()
}
process.on("SIGINT", stop)
process.on("SIGTERM", stop)

try {
    const uiStarted = performance.now()
    await waitFor(devUrl, 15_000)
    console.log(`dev: UI server ready in ${((performance.now() - uiStarted) / 1000).toFixed(2)}s`)
    game = await launchGame(process.argv.slice(2))
    process.exitCode = await game.exited
} finally {
    stop()
}
