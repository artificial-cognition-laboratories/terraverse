// Publishes a release: pushes the commit and tags it with package.json's
// version. The tag is what builds it: .github/workflows/release.yml.
import { releaseTag, run } from "./version"

const tag = releaseTag()
if ((await run(["git", "tag", "--list", tag])) !== "")
    throw new Error(`release: ${tag} is already tagged here — bump "version" in package.json`)
if ((await run(["git", "ls-remote", "--tags", "origin", `refs/tags/${tag}`])) !== "")
    throw new Error(`release: ${tag} already exists on GitHub — bump "version" in package.json`)

await run(["git", "push", "origin", "HEAD"])
await run(["git", "tag", "-a", tag, "-m", `Terraverse ${tag}`])
await run(["git", "push", "origin", tag])
console.log(`release: pushed ${tag}; GitHub Actions is building it`)
