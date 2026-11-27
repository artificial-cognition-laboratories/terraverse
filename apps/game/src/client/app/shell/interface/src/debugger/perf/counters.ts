/**
 * The old profiler's values, grouped for the Live charts and the Stats table.
 * A table of name prefixes, first match wins; a value no row claims goes in
 * Other until someone files it here.
 */
export const GROUPS = [
    { id: "map", title: "Map", prefixes: ["MapBlocks", "MapBlock ", "CM::", "SHADOW ", "renderMap"] },
    { id: "draws", title: "Draw calls", prefixes: ["Draws:"] },
    { id: "triangles", title: "Triangles", prefixes: ["Triangles:"] },
    { id: "gpu", title: "GPU", prefixes: ["Irr:"] },
    { id: "objects", title: "Objects", prefixes: ["ActiveObjectMgr", "ParticleManager"] },
    { id: "server", title: "Server env", prefixes: ["ServerEnv", "Server:", "Server::", "SEnv"] },
    { id: "meshing", title: "Meshing", prefixes: ["MeshUpdateQueue", "num_processed_meshes", "Client: Mesh"] },
    { id: "network", title: "Network", prefixes: ["client_received_packets", "packets_lost", "Net:"] },
    { id: "water", title: "Water", prefixes: ["Water"] },
    { id: "drawlist", title: "Draw list blocks", prefixes: ["Draw list:"] },
    { id: "far", title: "Far view tiles", prefixes: ["Far view: tiles", "Far view: stand-ins"] },
    { id: "emerge", title: "Emerge", prefixes: ["EmergeThread", "ServerMap"] },
] as const

export type GroupId = (typeof GROUPS)[number]["id"] | "other"

export function groupOf(name: string): { id: GroupId; title: string } {
    return GROUPS.find((g) => g.prefixes.some((p) => name.startsWith(p))) ?? { id: "other", title: "Other" }
}

/** Units that are time: those values are zones already, so Live leaves them to the zone charts. */
export const TIME_UNITS = new Set(["us", "ms", "s"])

/** What a chart draws through time: a frame, a memory sample. ms since the engine started. */
export type Timed = { start: number; end: number }

/** Bytes as they read best: MB under a gigabyte, GB over. */
export function formatBytes(bytes: number) {
    if (!Number.isFinite(bytes)) return "–"
    const abs = Math.abs(bytes)
    if (abs >= 1e9) return `${(bytes / 1e9).toFixed(2)} GB`
    if (abs >= 1e6) return `${(bytes / 1e6).toFixed(abs >= 100e6 ? 0 : 1)} MB`
    return `${(bytes / 1e3).toFixed(0)} kB`
}

/** A value for reading: counts whole, bytes in MB or GB, the rest to a sensible precision. */
export function formatValue(value: number, unit: string) {
    if (!Number.isFinite(value)) return "–"
    if (unit === "B") return formatBytes(value)
    if (unit === "#" || (unit === "" && Number.isInteger(value))) return Math.round(value).toLocaleString()
    const text = Math.abs(value) >= 100 ? value.toFixed(0) : Math.abs(value) >= 10 ? value.toFixed(1) : value.toFixed(2)
    return unit && unit !== "#" ? `${text} ${unit}` : text
}
