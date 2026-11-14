// Publishes a release: pushes the commit and tags it with package.json's
// version. The tag is what builds it: .github/workflows/release.yml.
// If that version is already tagged, the patch is bumped and committed first.
import { bumpPatch, releaseTag, run } from "./version"

async function taken(tag: string): Promise<boolean> {
    return (await run(["git", "tag", "--list", tag])) !== ""
        || (await run(["git", "ls-remote", "--tags", "origin", `refs/tags/${tag}`])) !== ""
}

let tag = releaseTag()
if (await taken(tag)) {
    const from = tag
    while (await taken(tag)) tag = bumpPatch()
    await run(["git", "commit", "-m", `Release ${tag}`, "--", "package.json"])
    console.log(`release: ${from} was taken; bumped to ${tag}`)
}

await run(["git", "push", "origin", "HEAD"])
await run(["git", "tag", "-a", tag, "-m", `Terraverse ${tag}`])
await run(["git", "push", "origin", tag])
console.log(`release: pushed ${tag}; GitHub Actions is building it`)
