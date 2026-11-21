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

/** A value for reading: counts whole, the rest to a sensible precision. */
export function formatValue(value: number, unit: string) {
    if (!Number.isFinite(value)) return "–"
    if (unit === "#" || (unit === "" && Number.isInteger(value))) return Math.round(value).toLocaleString()
    const text = Math.abs(value) >= 100 ? value.toFixed(0) : Math.abs(value) >= 10 ? value.toFixed(1) : value.toFixed(2)
    return unit && unit !== "#" ? `${text} ${unit}` : text
}
