#!/usr/bin/env bun
// Deploys the site and install.sh to Cloud Run, in the same project as axon-api:
// `arc deploy site`. It's a prerendered static site, so it takes no secrets or env.
import { $ } from "bun"
import { cp, mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { join } from "node:path"

const PROJECT = process.env.AXON_GCP_PROJECT_ID ?? "axon-487515"
const REGION = process.env.AXON_GCP_REGION ?? "europe-west1"
const SERVICE = process.env.TERRAVERSE_SITE_SERVICE ?? "terraverse-site"
const IMAGE = `gcr.io/${PROJECT}/${SERVICE}:latest`
const here = import.meta.dir
const release = join(here, "../../scripts/release")

await $`bun run build`.cwd(here)

const stage = await mkdtemp(join(tmpdir(), "terraverse-site-"))
try {
    const site = join(stage, "site")
    await mkdir(site)
    await cp(join(here, ".output/public"), site, { recursive: true })
    for (const file of ["Dockerfile", "nginx.conf"]) await cp(join(here, file), join(stage, file))

    // install.sh carries the key the game trusts; refuse to ship one that disagrees
    const key = (await readFile(join(release, "update-key.pub"), "utf8")).trim()
    const install = await readFile(join(release, "install.sh"), "utf8")
    if (!install.includes(key)) throw new Error("deploy: install.sh's PUBLIC_KEY isn't scripts/release/update-key.pub")
    await writeFile(join(site, "install.sh"), install)

    await $`docker build -t ${IMAGE} ${stage}`
    await $`docker push ${IMAGE}`
    await $`gcloud run deploy ${SERVICE} --image ${IMAGE} --region ${REGION} --project ${PROJECT} --allow-unauthenticated --port 8080 --quiet`
} finally {
    await rm(stage, { recursive: true, force: true })
}
