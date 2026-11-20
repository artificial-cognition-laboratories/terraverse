import { onMounted, onUnmounted, watch } from "vue"
import { useBridge } from "../bridge/use"
import { audioLevels, publishAudioLevels } from "../settings/audio"

/** A single menu track, owned by the menu so it stops as soon as play begins. */
export function useMenuMusic(enabled: boolean) {
    if (!enabled) return
    const bridge = useBridge()
    let player: HTMLAudioElement | null = null
    let pendingGesture = false
    let gone = false

    function volume() {
        if (!player || !audioLevels.value) return
        const levels = audioLevels.value
        const focus = document.hasFocus() ? 1 : levels.background
        player.volume = levels.muted ? 0 : Math.min(1, levels.master * levels.music * focus)
    }
    function clearGesture() {
        if (!pendingGesture) return
        pendingGesture = false
        window.removeEventListener("pointerdown", start)
        window.removeEventListener("keydown", start)
    }
    function start() {
        if (!player || gone) return
        void player.play().then(clearGesture).catch((error: unknown) => {
            if (error instanceof DOMException && error.name === "NotAllowedError") {
                if (pendingGesture) return
                pendingGesture = true
                window.addEventListener("pointerdown", start)
                window.addEventListener("keydown", start)
            } else if (!gone) console.error("menu music", error)
        })
    }
    async function refresh() {
        try {
            const settings = await bridge.send("settings.get", {})
            if (!gone) publishAudioLevels(settings)
        } catch (error) { if (!gone) console.error("menu audio settings", error) }
    }

    const stopWatch = watch(audioLevels, volume)
    onMounted(() => {
        // Production remains silent until the track's redistribution rights are documented.
        if (!import.meta.env.DEV) return
        player = new Audio("/menu-music/gaia.mp3")
        player.loop = true
        player.preload = "auto"
        player.volume = 0
        void refresh().then(() => { volume(); start() })
        window.addEventListener("focus", refresh)
        window.addEventListener("blur", volume)
    })
    onUnmounted(() => {
        gone = true
        stopWatch()
        clearGesture()
        window.removeEventListener("focus", refresh)
        window.removeEventListener("blur", volume)
        player?.pause()
        player = null
    })
}
