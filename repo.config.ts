// How `arc` releases Terra Firma (arclabs: libs/repo, which finds this file
// from the working directory). No import: defineRepo returns what it's given,
// and this repo doesn't depend on the ArcLabs workspace.
//
//   1. set "version" in package.json, commit
//   2. arc deploy release
// The parts of arc's release API used here (arclabs: libs/repo/src/config.ts).
type Step = unknown
type Target = { gate(steps: Step[]): void; publish(step: Step): void; verify(steps: Step[]): void }
type Release = { target(input: { cwd: string; rationale: string }): Target; script(name: string): Step }

export default {
    releaseBranch: "master",
    deploy: {
        release(release: Release) {
            const game = release.target({
                cwd: ".",
                rationale: "GitHub Releases: a tag builds every platform in CI. A tag can't be taken back once friends have downloaded it, so the build, tests and a headless world boot pass here first.",
            })
            game.gate([release.script("release:gate")])
            game.publish(release.script("release:tag"))
            game.verify([release.script("release:wait")])
            return game
        },
        docs(release: Release) {
            const docs = release.target({
                cwd: "apps/docs",
                rationale: "The landing page and install.sh on Cloud Run. Everyone's install runs this script, so its checks pass first and the deploy is proved against what's served.",
            })
            docs.gate([release.script("check")])
            docs.publish(release.script("deploy"))
            docs.verify([release.script("verify")])
            return docs
        },
    },
}
