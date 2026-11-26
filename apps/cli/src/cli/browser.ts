/** The command that opens a URL in the desktop's browser, by platform. */
const OPENERS: Record<string, readonly string[]> = {
    darwin: ["open"],
    win32: ["cmd", "/c", "start", ""],
    linux: ["xdg-open"],
}

/**
 * The person's browser: opens a link for them, when there's a desktop to open it on. Over SSH or
 * with no display there isn't, and the printed link is the way in.
 */
export function Browser() {
    function reachable(): boolean {
        if (process.env.SSH_CONNECTION || process.env.SSH_TTY) return false
        if (process.platform === "linux" && !process.env.DISPLAY && !process.env.WAYLAND_DISPLAY) return false
        return process.platform in OPENERS
    }

    /** Opens `url`, or says why it couldn't. Detached: terra never waits on the browser. */
    function open(url: string): { opened: true } | { opened: false; why: string } {
        if (!reachable()) return { opened: false, why: "no desktop here" }
        const opener = OPENERS[process.platform]!
        try {
            const child = Bun.spawn([...opener, url], { stdin: "ignore", stdout: "ignore", stderr: "ignore" })
            child.unref()
            return { opened: true }
        } catch (error) {
            return { opened: false, why: error instanceof Error ? error.message : String(error) }
        }
    }

    return { open: open }
}
export type Browser = ReturnType<typeof Browser>
