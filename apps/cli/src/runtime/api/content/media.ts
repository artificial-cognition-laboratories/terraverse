import { existsSync, mkdirSync, writeFileSync } from "node:fs"
import { err } from "../../../errors"
import { png, type Raster } from "../../../image"
import type { ManifestMod } from "../../../terra"

export type MediaFile = { readonly name: string; readonly path: string }

/**
 * Media — the files mods' content refers to, sent to players. The engine's media are
 * one flat namespace, so each mod's are named under its prefix: kilns--kiln.png. A texture
 * is a file in the mod's assets/textures/, or one art drew (named by what's in it, so a
 * changed picture is a new file, and the engine sees new content).
 */
export function Media(_opts: Record<string, never>) {
    const files = new Map<string, MediaFile>()
    /** The textures art drew, by the handle the mod holds: their media names. */
    const drawn = new WeakMap<object, string>()

    /** A texture of the mod's, a file or a handle art.texture gave: its media name. */
    function texture(mod: ManifestMod, value: unknown): string {
        if (typeof value === "object" && value !== null) {
            const name = drawn.get(value)
            if (!name) throw err("ASSET_MISSING", { detail: "a texture is a file in assets/textures/, or what art.texture returns" })
            return name
        }
        if (typeof value !== "string" || value === "" || value.includes("..") || value.startsWith("/"))
            throw err("ASSET_MISSING", { detail: `a texture is a file in assets/textures/, not ${JSON.stringify(value)}` })
        const path = `${mod.root}/assets/textures/${value}`
        if (!existsSync(path)) throw err("ASSET_MISSING", { detail: `${mod.name} has no assets/textures/${value}` })
        const name = `${mod.prefix}--${value.replaceAll("/", "--")}`
        files.set(name, { name: name, path: path })
        return name
    }

    /** A picture art painted, written into the mod's built folder: the handle that names it. */
    function draw(mod: ManifestMod, picture: Raster): object {
        const bytes = png.encode(picture)
        const name = `${mod.prefix}--art-${Bun.hash(bytes).toString(16).padStart(16, "0")}.png`
        const path = `${mod.root}/art/${name}`
        if (!files.has(name)) {
            mkdirSync(`${mod.root}/art`, { recursive: true })
            writeFileSync(path, bytes)
            files.set(name, { name: name, path: path })
        }
        const handle = Object.freeze({ size: picture.width })
        drawn.set(handle, name)
        return handle
    }

    function list(): MediaFile[] {
        return [...files.values()]
    }

    return { texture: texture, drawn: draw, list: list }
}

export type MediaT = ReturnType<typeof Media>
