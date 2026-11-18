import { existsSync } from "node:fs"
import { err } from "../../../errors"
import type { ManifestMod } from "../../../terra"

export type MediaFile = { readonly name: string; readonly path: string }

/**
 * Media — the files mods' content refers to, sent to players. The engine's media are
 * one flat namespace, so each mod's are named under its prefix: kilns--kiln.png.
 */
export function Media(_opts: Record<string, never>) {
    const files = new Map<string, MediaFile>()

    /** A texture of the mod's, by its path in assets/textures/: its media name. */
    function texture(mod: ManifestMod, file: unknown): string {
        if (typeof file !== "string" || file === "" || file.includes("..") || file.startsWith("/"))
            throw err("ASSET_MISSING", { detail: `a texture is a file in assets/textures/, not ${JSON.stringify(file)}` })
        const path = `${mod.root}/assets/textures/${file}`
        if (!existsSync(path)) throw err("ASSET_MISSING", { detail: `${mod.name} has no assets/textures/${file}` })
        const name = `${mod.prefix}--${file.replaceAll("/", "--")}`
        files.set(name, { name: name, path: path })
        return name
    }

    function list(): MediaFile[] {
        return [...files.values()]
    }

    return { texture: texture, list: list }
}

export type MediaT = ReturnType<typeof Media>
