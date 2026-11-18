import { existsSync, statSync } from "node:fs"
import { err } from "../../errors"

type InstallOpts = {
    root: string
    /** The program to run as bun: this terra, which is bun too (BUN_BE_BUN). */
    bun: string
}

/** Install — brings a folder's node_modules up to its package.json, with the bun inside terra. */
export function Install(opts: InstallOpts) {
    /** Whether package.json has changed since the last install. */
    function needed(): boolean {
        const lock = `${opts.root}/bun.lock`
        if (!existsSync(`${opts.root}/node_modules`) || !existsSync(lock)) return true
        return statSync(`${opts.root}/package.json`).mtimeMs > statSync(lock).mtimeMs
    }

    async function run(): Promise<void> {
        const proc = Bun.spawn([opts.bun, "install"], {
            cwd: opts.root,
            env: { ...process.env, BUN_BE_BUN: "1" },
            stdout: "pipe",
            stderr: "pipe",
        })
        const [code, out, error] = await Promise.all([proc.exited, new Response(proc.stdout).text(), new Response(proc.stderr).text()])
        if (code !== 0) throw err("INSTALL_FAILED", { detail: (error || out).trim() })
    }

    return { needed: needed, run: run }
}

export type InstallT = ReturnType<typeof Install>
