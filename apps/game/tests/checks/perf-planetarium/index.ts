// Where the Planetarium spends its time, step by step through a script (open a
// world, zoom in, pan, zoom out, zoom in again), cold (a new home: nothing kept)
// then warm (opened again), as `bun perf:planetarium`. Each run's a line in
// .output/perf/planetarium.jsonl, and the last like it is shown beside it.
//   [CODE=TF24.eHRqNGM3aHc.BAKAqoCAAWZAgICAgICAgICCVQ] [SCRIPT=zoom|open] [CELL=2] [CPUS=8] [SHOTS=1] bun perf:planetarium
// CPUS pins it to that many cores (taskset), standing in for a smaller machine.
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync, appendFileSync, existsSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { Table } from "./table"

const root = (await Bun.$`git rev-parse --show-toplevel`.text()).trim()
const env = process.env
const opts = {
    // a world as players make them: the engine's default code is a small one,
    // its peaks under the camera's lowest, which hid holes and memory
    code: env.CODE ?? "TF24.eHRqNGM3aHc.BAKAqoCAAWZAgICAgICAgICCVQ",
    script: env.SCRIPT ?? "zoom",
    cell: env.CELL,
    cpus: cores(env.CPUS),
    shots: env.SHOTS === "1",
}
const out = join(root, ".output/perf")

// CPUS is a count of cores; anything else would silently run unpinned
function cores(given: string | undefined) {
    if (given === undefined) return undefined
    if (!/^[1-9][0-9]*$/.test(given)) throw new Error(`CPUS is a count of cores (CPUS=4), not "${given}"`)
    return Number(given)
}
const log = join(out, "planetarium.jsonl")
mkdirSync(out, { recursive: true })
const home = mkdtempSync(join(tmpdir(), "planetarium-probe-"))
const conf = join(home, "probe.conf")
writeFileSync(conf, "screen_w = 640\nscreen_h = 360\nfullscreen = false\n")
const rev = (await Bun.$`git -C ${root} rev-parse --short HEAD`.text()).trim()
const dirty = (await Bun.$`git -C ${root} status --porcelain -- apps/game/src apps/game/library`.text()).trim() !== ""

try {
    for (const warm of [false, true]) {
        const label = warm ? "warm" : "cold"
        const json = join(home, `${label}.json`)
        const shots = join(out, "planetarium-shots", label)
        if (opts.shots) rmSync(shots, { recursive: true, force: true })
        const args = [
            join(root, ".output/bin/terraverse"), "--config", conf,
            "--planetarium", opts.code, "--planetarium-probe", json, "--planetarium-script", opts.script,
            ...(opts.cell ? ["--planetarium-cell", opts.cell] : []),
            ...(opts.shots ? ["--planetarium-shots", shots] : []),
        ]
        const pinned = opts.cpus ? ["taskset", "-c", `0-${opts.cpus - 1}`, ...args] : args
        console.log(`== ${label}${opts.cpus ? `, ${opts.cpus} cores` : ""}`)
        const run = Bun.spawnSync(pinned, { env: { ...env, TERRAVERSE_HOME: join(home, "home") }, stdout: "pipe", stderr: "pipe" })
        if (run.exitCode !== 0 || !existsSync(json)) {
            console.error(run.stderr.toString().split("\n").slice(-30).join("\n"))
            throw new Error(`the probe failed (${label}, exit ${run.exitCode})`)
        }
        const report = JSON.parse(readFileSync(json, "utf8"))
        const line = { when: new Date().toISOString(), rev: rev, dirty: dirty, warm: warm, pinned: opts.cpus ?? null, ...report }
        const last = previous(line)
        appendFileSync(log, JSON.stringify(line) + "\n")
        console.log(Table({ report: line, last: last }).text())
        if (opts.shots) console.log(`   pictures: ${shots}`)
    }
} finally {
    rmSync(home, { recursive: true, force: true })
}

// The last run like this one: the same world, script, cell, cores and temperature
function previous(run: Record<string, unknown>) {
    if (!existsSync(log)) return undefined
    const same = ["code", "script", "cell", "cores", "warm", "pinned"]
    const lines = readFileSync(log, "utf8").trim().split("\n").filter(l => l)
    for (const l of lines.reverse()) {
        const was = JSON.parse(l)
        if (same.every(k => was[k] === run[k])) return was
    }
    return undefined
}
