/**
 * What each platform is offered, as data: the landing reads this and nothing
 * else. A platform without a game build says so rather than offering a
 * launcher that would find nothing to install.
 */
const ROLLING = "https://github.com/artificial-cognition-laboratories/terraverse/releases/download/launcher"

export type PlatformId = "linux" | "windows" | "macos"

export interface Download {
    label: string
    /** What it is, in a few words */
    detail: string
    href: string
}

export interface Platform {
    id: PlatformId
    label: string
    icon: string
    /** The one-line install, when there is one */
    command?: string
    downloads: Download[]
    /** Set while there's nothing to download; shown instead */
    notReady?: string
}

export const platforms: Platform[] = [
    {
        id: "linux",
        label: "Linux",
        icon: "simple-icons:linux",
        command: "curl -fsSL https://terraverse.arclabs.it/install.sh | sh",
        downloads: [
            { label: ".deb", detail: "Debian, Ubuntu, Mint", href: `${ROLLING}/terraverse-launcher_amd64.deb` },
            { label: "AppImage", detail: "any x86_64 distro", href: `${ROLLING}/terraverse-launcher-x86_64.AppImage` },
        ],
    },
    {
        id: "windows",
        label: "Windows",
        icon: "simple-icons:windows",
        // The launcher builds for Windows (.github/workflows/launcher.yml), but
        // there's no Windows game build for it to install yet. When release.yml
        // ships one, drop notReady.
        downloads: [{ label: "Installer", detail: "Windows 10 and 11, x64", href: `${ROLLING}/terraverse-launcher-setup.exe` }],
        notReady: "The Windows build is on its way.",
    },
    {
        id: "macos",
        label: "macOS",
        icon: "simple-icons:apple",
        downloads: [],
        notReady: "macOS isn't built yet.",
    },
]
