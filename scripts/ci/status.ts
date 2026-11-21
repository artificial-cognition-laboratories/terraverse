// CI at a glance, from GitHub through `gh`: what's running, what's red and where, and
// the CI owner's Scoreboard (.agents/owners/ci/charter.md) over recent master runs.
//   bun ci:status            the last 30 runs of each workflow
//   bun ci:status --since 2026-11-17
// Needs `gh` signed in. Reads only; it never starts, cancels or reruns anything.

import { existsSync } from "node:fs"

type Step = { name: string; conclusion: string | null; status: string }
type Job = { name: string; conclusion: string | null; status: string; startedAt: string; completedAt: string; steps: Step[] }
type Run = {
    databaseId: number
    workflowName: string
    headBranch: string
    headSha: string
    event: string
    status: string
    conclusion: string | null
    createdAt: string
    updatedAt: string
    displayTitle: string
}

const args = process.argv.slice(2)
const sinceArg = args.includes("--since") ? args[args.indexOf("--since") + 1] : undefined
if (args.includes("--since") && !sinceArg) throw new Error("ci:status: --since needs a date (YYYY-MM-DD)")
const since = sinceArg ? Date.parse(sinceArg) : undefined
if (since !== undefined && Number.isNaN(since)) throw new Error(`ci:status: --since ${sinceArg} isn't a date`)

function gh<T>(...argv: string[]): T {
    const proc = Bun.spawnSync(["gh", ...argv], { stdout: "pipe", stderr: "pipe" })
    if (proc.exitCode !== 0) throw new Error(`gh ${argv.join(" ")} failed: ${proc.stderr.toString().trim()}`)
    return JSON.parse(proc.stdout.toString()) as T
}

const RUN_FIELDS = "databaseId,workflowName,headBranch,headSha,event,status,conclusion,createdAt,updatedAt,displayTitle"
// Only workflows still in the repo: GitHub keeps listing deleted ones (and scratch branches') as active.
const root = `${import.meta.dir}/../..`
const workflows = gh<{ name: string; state: string; path: string }[]>("workflow", "list", "--json", "name,state,path")
    .filter(w => w.state === "active" && existsSync(`${root}/${w.path}`))
    .map(w => w.name)
let runs: Run[] = workflows.flatMap(w => gh<Run[]>("run", "list", "--workflow", w, "-L", "30", "--json", RUN_FIELDS))
if (since !== undefined) runs = runs.filter(r => Date.parse(r.createdAt) >= since)
runs.sort((a, b) => Date.parse(a.createdAt) - Date.parse(b.createdAt))

const jobsCache = new Map<number, Job[]>()
function jobs(run: Run): Job[] {
    let found = jobsCache.get(run.databaseId)
    if (!found) {
        found = gh<{ jobs: Job[] }>("run", "view", String(run.databaseId), "--json", "jobs").jobs
        jobsCache.set(run.databaseId, found)
    }
    return found
}

const ms = (a: string, b: string) => Date.parse(b) - Date.parse(a)
const min = (n: number) => `${(n / 60000).toFixed(1)} min`
const median = (xs: number[]) => {
    if (xs.length === 0) return undefined
    const s = [...xs].sort((a, b) => a - b)
    const m = Math.floor(s.length / 2)
    return s.length % 2 ? s[m] : (s[m - 1]! + s[m]!) / 2
}
const when = (iso: string) => iso.replace("T", " ").slice(5, 16)

// ── now ──────────────────────────────────────────────────────────────────────
console.log("## Now")
const live = runs.filter(r => r.status !== "completed")
if (live.length === 0) console.log("nothing running")
for (const r of live) console.log(`running  ${r.workflowName} on ${r.headBranch} (${r.event}) ${r.headSha.slice(0, 8)}, ${min(Date.now() - Date.parse(r.createdAt))} so far  #${r.databaseId}`)

// The latest finished run of each workflow on master, and where it failed.
console.log("\n## Master, latest of each workflow")
for (const w of workflows) {
    const last = runs.filter(r => r.workflowName === w && r.headBranch === "master" && r.status === "completed" && r.conclusion !== "cancelled").at(-1)
    if (!last) { console.log(`${w}: no finished master run`); continue }
    console.log(`${last.conclusion === "success" ? "green" : "RED  "}  ${w}  ${last.headSha.slice(0, 8)} ${when(last.createdAt)}  "${last.displayTitle}"  #${last.databaseId}`)
    if (last.conclusion !== "success")
        for (const j of jobs(last).filter(j => j.conclusion === "failure"))
            console.log(`       ${j.name}: failed at "${j.steps.find(s => s.conclusion === "failure")?.name ?? "?"}"  (gh run view ${last.databaseId} --log-failed)`)
}

// ── the Scoreboard ───────────────────────────────────────────────────────────
// Master push runs. A cancelled run is superseded, and doesn't count, when a newer run of the
// same workflow on master (pushed or dispatched: they share a concurrency group) started before
// it ended; cancelled otherwise counts as red. Newer is by run id: two can start in one second.
const master = runs.filter(r => r.headBranch === "master" && r.event === "push" && r.status === "completed")
const superseded = (r: Run) => r.conclusion === "cancelled" &&
    runs.some(o => o.workflowName === r.workflowName && o.headBranch === "master" && o.databaseId > r.databaseId && Date.parse(o.createdAt) <= Date.parse(r.updatedAt))
const counted = master.filter(r => !superseded(r))
const green = counted.filter(r => r.conclusion === "success").length

console.log("\n## Scoreboard")
const span = master.length ? `${when(master[0]!.createdAt)} to ${when(master.at(-1)!.updatedAt)}` : "no runs"
console.log(`Master green rate: ${green}/${counted.length}${counted.length ? ` = ${Math.round(100 * green / counted.length)}%` : ""}  (${span}; ${master.length - counted.length} superseded cancels excluded)`)

// Time to signal: per pushed commit, from its first run starting to the first red, or to the
// last run finishing when all are green. Superseded commits have no signal and are left out.
const bySha = Map.groupBy(counted, r => r.headSha)
const signals: number[] = []
for (const rs of bySha.values()) {
    const start = Math.min(...rs.map(r => Date.parse(r.createdAt)))
    const red = rs.filter(r => r.conclusion !== "success").map(r => Date.parse(r.updatedAt))
    const end = red.length ? Math.min(...red) : Math.max(...rs.map(r => Date.parse(r.updatedAt)))
    signals.push(end - start)
}
const signal = median(signals)
console.log(`Time to signal (median): ${signal === undefined ? "no runs" : min(signal)} over ${signals.length} pushes`)

// Build time per workflow and platform job: successful jobs on master pushes and tags. Whether
// a job's caches hit isn't in the API, so a job is counted cold when it took over twice the
// median of its kind; the warm figure is the median of the rest.
const built = runs.filter(r => r.status === "completed" && r.conclusion === "success" && (r.headBranch === "master" || r.event === "push"))
const durations = new Map<string, { at: string; ms: number }[]>()
for (const r of built)
    for (const j of jobs(r))
        if (j.conclusion === "success" && j.startedAt && j.completedAt) {
            const key = `${r.workflowName}/${j.name}`
            durations.set(key, [...(durations.get(key) ?? []), { at: r.createdAt, ms: ms(j.startedAt, j.completedAt) }])
        }
console.log("Warm / cold build (successful jobs; cold = over 2x the median):")
for (const [key, ds] of [...durations].sort()) {
    const all = median(ds.map(d => d.ms))!
    const cold = ds.filter(d => d.ms > 2 * all)
    const warm = median(ds.filter(d => d.ms <= 2 * all).map(d => d.ms))!
    console.log(`  ${key.padEnd(24)} warm ${min(warm)} (${ds.length - cold.length} runs)   cold ${cold.length ? `${min(cold.at(-1)!.ms)} (${when(cold.at(-1)!.at)})` : "none in window"}`)
}

// Release reliability: tags whose run published on its first attempt.
const tags = runs.filter(r => r.event === "push" && r.headBranch !== "master" && /^(v|launcher-v)\d/.test(r.headBranch))
const firstTry = (r: Run) => r.conclusion === "success" && gh<{ attempt: number }>("run", "view", String(r.databaseId), "--json", "attempt").attempt === 1
const recent = tags.slice(-5)
console.log(`Release reliability: ${recent.filter(firstTry).length}/${recent.length} of the last tags published first time (${recent.map(t => `${t.headBranch} ${t.conclusion}`).join(", ") || "none"})`)

// Runner minutes per day, over every run listed: the sum of its jobs' durations.
const allJobs = runs.filter(r => r.status === "completed").flatMap(jobs).filter(j => j.startedAt && j.completedAt && j.completedAt > j.startedAt)
const total = allJobs.reduce((s, j) => s + ms(j.startedAt, j.completedAt), 0)
const days = runs.length ? Math.max(1, ms(runs[0]!.createdAt, runs.at(-1)!.updatedAt) / 86400000) : 1
console.log(`Runner minutes per day: ${(total / 60000 / days).toFixed(0)} (${(total / 60000).toFixed(0)} min over ${days.toFixed(1)} days, ${runs.length} runs)`)
