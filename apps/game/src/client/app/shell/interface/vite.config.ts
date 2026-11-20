import { defineConfig } from "vite"
import vue from "@vitejs/plugin-vue"
import { createReadStream, statSync } from "node:fs"
import { fileURLToPath } from "node:url"

const gaia = fileURLToPath(new URL("../../../../../assets/music/if-gaia-was-one-of-us-part-four-denovaire-main-version-25244-02-27.mp3", import.meta.url))

// Relative base so the built app loads from file:// inside the engine.
export default defineConfig({
    base: "./",
    // Two pages in the engine: the game's, and the debugger's beside it (shell/debugger.h).
    build: {
        outDir: "../../../../../../../.output/ui/dist", emptyOutDir: true,
        rollupOptions: { input: { index: "index.html", debugger: "debugger.html" } },
    },
    plugins: [vue(), {
        name: "local-menu-music",
        configureServer(server) {
            // The local preview can play the supplied track without copying it into release builds.
            server.middlewares.use("/menu-music/gaia.mp3", (request, response) => {
                const size = statSync(gaia).size
                const range = request.headers.range?.match(/^bytes=(\d+)-(\d*)$/)
                const start = range ? Number(range[1]) : 0
                const end = range && range[2] ? Number(range[2]) : size - 1
                if (start >= size || end < start || end >= size) {
                    response.writeHead(416, { "Content-Range": `bytes */${size}` })
                    response.end()
                    return
                }
                response.writeHead(range ? 206 : 200, {
                    "Content-Type": "audio/mpeg",
                    "Accept-Ranges": "bytes",
                    "Content-Length": end - start + 1,
                    ...(range ? { "Content-Range": `bytes ${start}-${end}/${size}` } : {}),
                })
                createReadStream(gaia, { start, end }).pipe(response)
            })
        },
    }],
})
