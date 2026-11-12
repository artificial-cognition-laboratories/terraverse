// How `arc` releases Terra Firma (arclabs: libs/repo, which finds this file
// from the working directory). No import: defineRepo returns what it's given,
// and this repo doesn't depend on the ArcLabs workspace.
//
//   1. set "version" in package.json, commit
//   2. arc deploy terraverse
//
// The launcher (apps/launcher) releases on its own, more rarely: bump its
// version in tauri.conf.json and Cargo.toml, then arc deploy launcher.
// The site (apps/site) deploys on its own: arc deploy site.
// The parts of arc's release API used here (arclabs: libs/repo/src/config.ts).
type Step = unknown
type Target = { gate(steps: Step[]): void; publish(step: Step): void; verify(steps: Step[]): void }
type Release = { target(input: { cwd: string; rationale: string }): Target; script(name: string): Step }

export default {
    releaseBranch: "master",
    deploy: {
        terraverse(release: Release) {
            const game = release.target({
                cwd: ".",
                rationale: "GitHub Releases: a tag builds every platform in CI. A tag can't be taken back once friends have downloaded it, so the build, tests and a headless world boot pass here first.",
            })
            game.gate([release.script("release:gate")])
            game.publish(release.script("release:tag"))
            game.verify([release.script("release:wait")])
            return game
        },
        launcher(release: Release) {
            const launcher = release.target({
                cwd: "apps/launcher",
                rationale: "GitHub Releases: a launcher-v tag builds it in CI, and every installed launcher updates itself to it. A broken launcher can't fix itself, so its tests pass here first and the release is proved when the updater points at it.",
            })
            launcher.gate([release.script("release:gate")])
            launcher.publish(release.script("release:tag"))
            launcher.verify([release.script("release:wait")])
            return launcher
        },
        site(release: Release) {
            const site = release.target({
                cwd: "apps/site",
                rationale: "The site and install.sh on Cloud Run. Everyone's install runs this script, so its checks pass first and the deploy is proved against what's served.",
            })
            site.gate([release.script("check")])
            site.publish(release.script("deploy"))
            site.verify([release.script("verify")])
            return site
        },
    },
}
