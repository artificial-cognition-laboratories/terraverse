// Checks what Cloud Run is actually serving after a deploy.
import { $ } from "bun"

const url = (await $`gcloud run services describe terraverse-site --region europe-west1 --project axon-487515 --format=${"value(status.url)"}`.text()).trim()
for (const [path, expect] of [["/", "<title>Terraverse</title>"], ["/install.sh", "#!/bin/sh"], ["/worlds/worlds.json", "TF16."]] as const) {
    const response = await fetch(url + path)
    const body = await response.text()
    if (!response.ok || !body.includes(expect)) throw new Error(`verify: ${path} answered ${response.status} without "${expect}"`)
    console.log(`ok: ${path}`)
}
console.log(`verify: ${url} is serving the site`)
