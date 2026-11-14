// The version being released: package.json's, as a tag (v0.2.0).
export function releaseTag(): string {
    const pkg: unknown = JSON.parse(require("fs").readFileSync(`${import.meta.dir}/../../package.json`, "utf8"))
    const version = typeof pkg === "object" && pkg !== null && "version" in pkg ? pkg.version : undefined
    if (typeof version !== "string" || !/^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$/.test(version))
        throw new Error(`release: package.json needs a "version" like 0.2.0 (has ${JSON.stringify(version)})`)
    return `v${version}`
}

/** Runs a command; throws with its output if it fails. */
export async function run(argv: string[]): Promise<string> {
    const proc = Bun.spawn(argv, { stdout: "pipe", stderr: "pipe" })
    const [out, err, code] = await Promise.all([new Response(proc.stdout).text(), new Response(proc.stderr).text(), proc.exited])
    if (code !== 0) throw new Error(`${argv.join(" ")} failed (${code}): ${err.trim() || out.trim()}`)
    return out.trim()
}

/** Bumps package.json's patch version (0.2.0 → 0.2.1, dropping any prerelease) and returns the new tag. */
export function bumpPatch(): string {
    const fs = require("fs")
    const path = `${import.meta.dir}/../../package.json`
    const [major, minor, patch] = releaseTag().slice(1).split("-")[0].split(".").map(Number)
    const next = `${major}.${minor}.${patch + 1}`
    const text: string = fs.readFileSync(path, "utf8")
    fs.writeFileSync(path, text.replace(/("version"\s*:\s*)"[^"]*"/, `$1"${next}"`))
    return releaseTag()
}
