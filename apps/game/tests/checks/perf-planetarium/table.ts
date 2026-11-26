// A probe's run as a table: each step's time, its frames, the tiles it took
// and threw away, and where the workers' and the main thread's time went,
// with the last like run's beside the headline numbers.

type Zone = { ms: number; max: number; count: number }
type Step = {
    name: string
    seconds: number
    moving: number
    settled: number | null
    lost: boolean
    frames: { count: number; p50?: number; p95?: number; max?: number; over16?: number; over50?: number }
    drawn: { tiles: number; draws: number; vertices: number }
    threads?: Record<string, { threads: number; zones?: Record<string, Zone> }>
    counters?: Record<string, { sum: number; max: number }>
}
type Report = {
    code: string; script: string; cell: number; width: number; height: number; cores: number
    rev: string; dirty: boolean; warm: boolean; when?: string
    steps: Step[]
    making: { seconds: number; steps: { name: string; seconds: number; kept: boolean }[] }
}

export function Table(opts: { report: Report; last: Report | undefined }) {
    const report = opts.report
    const last = opts.last

    function text(): string {
        const lines: string[] = []
        lines.push(`   ${report.code} · cell ${report.cell} px · ${report.width}×${report.height} · ${report.cores} cores · ${report.rev}${report.dirty ? "+" : ""}`
            + (last ? `   (last: ${last.rev}${last.dirty ? "+" : ""}, ${last.when?.slice(0, 16) ?? ""})` : ""))
        lines.push("")
        lines.push(row(["step", "time", "settle", "frames p50/p95/max", ">16ms", "built", "read", "again", "evicted", "holes max", "unready", "workers busy", "MB held"]))
        for (const step of report.steps) lines.push(row(cells(step, last?.steps.find(s => s.name === step.name))))
        lines.push("")
        for (const step of report.steps) lines.push(...where(step))
        lines.push("")
        lines.push("   making the world: " + report.making.steps.map(s => `${s.name} ${s.seconds.toFixed(2)}${s.kept ? " (kept)" : ""}`).join(" · "))
        return lines.join("\n")
    }

    return { text: text }
}

function cells(step: Step, was: Step | undefined): string[] {
    const count = (name: string) => step.counters?.[name]?.sum ?? 0
    const workers = step.threads?.["Far tiles"]
    const busy = workers ? (workers.zones?.["Far tile"]?.ms ?? 0) / (workers.threads * step.seconds * 1000) : 0
    const f = step.frames
    return [
        step.name,
        `${step.seconds.toFixed(2)}s` + delta(step.seconds, was?.seconds),
        step.settled === null ? "never" : `${step.settled.toFixed(2)}s`,
        f.count ? `${f.p50!.toFixed(1)}/${f.p95!.toFixed(1)}/${f.max!.toFixed(0)}` : "-",
        `${f.over16 ?? 0}`,
        `${count("Far tile: built")}` + delta(count("Far tile: built"), was?.counters?.["Far tile: built"]?.sum),
        `${count("Far tile: read")}`,
        `${count("Far view: tiles taken again")}`,
        `${count("Far view: tiles evicted")}`,
        `${step.counters?.["Far view: holes"]?.max ?? 0}`,
        `${step.counters?.["Far view: stand-ins not ready"]?.max ?? 0}`,
        workers ? `${(busy * 100).toFixed(0)}% of ${workers.threads}` : "-",
        `${(step.counters?.["Far view: MB held"]?.max ?? 0).toFixed(0)}`,
    ]
}

// each thread kind's biggest zones in the step
function where(step: Step): string[] {
    const out = [`   ${step.name}${step.lost ? " (some zones lost: a ring went round)" : ""}`]
    for (const [thread, t] of Object.entries(step.threads ?? {})) {
        const zones = Object.entries(t.zones ?? {}).sort((a, b) => b[1].ms - a[1].ms).slice(0, 6)
        if (!zones.length || zones[0]![1].ms < 5) continue
        out.push(`      ${thread}${t.threads > 1 ? ` ×${t.threads}` : ""}: ` + zones.map(([name, z]) => `${name} ${z.ms.toFixed(0)}ms (${z.count}, max ${z.max.toFixed(0)})`).join(" · "))
    }
    return out
}

function delta(now: number, was: number | undefined): string {
    if (was === undefined || was === 0) return ""
    const change = (now - was) / was
    return Math.abs(change) < 0.05 ? "" : ` ${change > 0 ? "+" : ""}${(change * 100).toFixed(0)}%`
}

function row(cells: string[]): string {
    const widths = [14, 14, 8, 20, 6, 12, 6, 6, 8, 10, 8, 13, 7]
    return "   " + cells.map((c, i) => c.padEnd(widths[i] ?? 8)).join(" ")
}
