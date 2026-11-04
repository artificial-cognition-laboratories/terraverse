#!/usr/bin/env bun
// Deploys the site and install.sh to Cloud Run, in the same project as axon-api:
// `arc deploy docs`. It's public static files, so it takes no secrets or env.
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

const stage = await mkdtemp(join(tmpdir(), "terraverse-site-"))
try {
    const site = join(stage, "site")
    await mkdir(site)
    for (const file of ["index.html", "site.css", "site.js", "planet.js", "fonts", "worlds"])
        await cp(join(here, file), join(site, file), { recursive: true })
    for (const file of ["Dockerfile", "nginx.conf"]) await cp(join(here, file), join(stage, file))

    // The key lives in one place: install.sh is stamped with it as it ships.
    const key = await readFile(join(release, "update-key.pub"), "utf8").catch(() => null)
    const placeholder = "NOT SET: paste scripts/release/update-key.pub here"
    let install = await readFile(join(release, "install.sh"), "utf8")
    if (!install.includes(placeholder)) throw new Error("deploy: install.sh's key placeholder has moved")
    if (key) install = install.replace(placeholder, key.trim())
    else console.warn("deploy: no update-key.pub yet, so the published install.sh will refuse to install")
    await writeFile(join(site, "install.sh"), install)

    await $`docker build -t ${IMAGE} ${stage}`
    await $`docker push ${IMAGE}`
    await $`gcloud run deploy ${SERVICE} --image ${IMAGE} --region ${REGION} --project ${PROJECT} --allow-unauthenticated --port 8080 --quiet`
} finally {
    await rm(stage, { recursive: true, force: true })
}
