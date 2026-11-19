/**
 * What each message type is about, for the Network tab's chips, charts and
 * colours. A table of name fragments (the name less its TOCLIENT_/TOSERVER_),
 * first match wins; a type no row claims is Other until it's filed here.
 */
export const CATEGORIES = [
    { id: "world", label: "World", color: "#5fbf72", parts: ["BLOCKDATA", "ADDNODE", "REMOVENODE", "NODEMETA", "TIME_OF_DAY", "GOTBLOCKS", "DELETEDBLOCKS", "SET_SKY", "SET_SUN", "SET_MOON", "SET_STARS", "CLOUD", "OVERRIDE_DAY_NIGHT", "SET_LIGHTING"] },
    { id: "objects", label: "Objects", color: "#4f9dff", parts: ["ACTIVE_OBJECT", "PARTICLE", "SPAWN_PARTICLE", "PLAY_SOUND", "STOP_SOUND", "FADE_SOUND"] },
    { id: "player", label: "Player", color: "#e5a33b", parts: ["PLAYERPOS", "MOVE_PLAYER", "HP", "BREATH", "DEATHSCREEN", "RESPAWN", "INTERACT", "PLAYER_SPEED", "LOCAL_PLAYER_ANIMATIONS", "EYE_OFFSET", "FOV", "PRIVILEGES", "MOVEMENT", "DAMAGE", "CAMERA"] },
    { id: "inventory", label: "Inventory", color: "#c678dd", parts: ["INVENTORY", "DETACHED_INVENTORY"] },
    { id: "ui", label: "UI", color: "#3fbfbf", parts: ["HUD", "FORMSPEC", "CHAT", "SHOW_", "MINIMAP", "SET_UI", "UI_EVENT"] },
    { id: "media", label: "Media", color: "#e88a5a", parts: ["MEDIA", "NODEDEF", "ITEMDEF"] },
    { id: "mods", label: "Mods", color: "#d987b8", parts: ["MODCHANNEL", "CSM_"] },
    { id: "connection", label: "Connection", color: "#8a8cf0", parts: ["HELLO", "INIT", "AUTH", "TICKET", "ACCESS_DENIED", "CLIENT_READY", "UPDATE_CLIENT_INFO", "DISCONNECT"] },
] as const

export type CategoryId = (typeof CATEGORIES)[number]["id"] | "other"
export const OTHER = { id: "other" as const, label: "Other", color: "#7c7b85" }

/** A type's name without its direction's prefix: "BLOCKDATA". */
export const short = (name: string) => name.replace(/^TO(CLIENT|SERVER)_/, "")

export function categoryOf(name: string): { id: CategoryId; label: string; color: string } {
    const bare = short(name)
    return CATEGORIES.find((c) => c.parts.some((p) => bare.includes(p))) ?? OTHER
}

/** Bytes for reading: B, kB, MB (1000s, as the rest of the page counts). */
export function formatBytes(bytes: number) {
    if (!Number.isFinite(bytes)) return "–"
    if (bytes < 1000) return `${Math.round(bytes)} B`
    if (bytes < 1e6) return `${(bytes / 1000).toFixed(bytes < 1e4 ? 1 : 0)} kB`
    return `${(bytes / 1e6).toFixed(1)} MB`
}
