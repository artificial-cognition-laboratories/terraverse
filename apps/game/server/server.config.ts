// The game's own server: the official mods every Terraverse world runs, beside the tf_* Lua
// mods until they're ported (sprint 119). Staged prepared into the runtime tree
// (scripts/stage-runtime.sh); the engine runs it in every world it isn't given another
// server folder for. Its worlds come from the menu, each with its own code: `system` is
// only for running this folder by hand (terra serve apps/game/server).
export default defineServer({
    title: "Terraverse",

    system: "TF24.cTNyM2ZwZDg.BAKAqoCAAWZAgICAgICAgICCVQ",

    access: {
        join: "friends",
    },
})
