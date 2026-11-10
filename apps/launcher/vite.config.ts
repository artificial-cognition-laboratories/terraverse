import { defineConfig } from "vite"
import vue from "@vitejs/plugin-vue"
import { fileURLToPath } from "node:url"

// The launcher wears the game's kit and theme, so the two read as one product.
const gameUi = fileURLToPath(new URL("../game/ui/src", import.meta.url))

export default defineConfig({
    plugins: [vue()],
    resolve: { alias: { "@game": gameUi } },
    clearScreen: false,
    // the kit, theme and fonts live under apps/game/ui: let the dev server read them
    server: { port: 5175, strictPort: true, fs: { allow: [".", "../game/ui"] } },
    build: { outDir: "dist", emptyOutDir: true },
})
