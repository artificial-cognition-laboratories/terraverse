// What only this check's fixture reaches for: the sidecar's process, to crash it once.
// Mods get the engine's globals and nothing else; this is a test, not a mod.
declare module "node:fs" {
    export function existsSync(path: string): boolean
    export function writeFileSync(path: string, data: string): void
}
declare const process: { exit(code: number): never }
interface ImportMeta { readonly dir: string }
