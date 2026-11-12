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
    /** Its tile's icon */
    icon: string
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
            { label: ".deb", detail: "Debian, Ubuntu", href: `${ROLLING}/terraverse-launcher_amd64.deb`, icon: "simple-icons:debian" },
            { label: "AppImage", detail: "any distro", href: `${ROLLING}/terraverse-launcher-x86_64.AppImage`, icon: "simple-icons:appimage" },
        ],
    },
    {
        id: "windows",
        label: "Windows",
        icon: "simple-icons:windows",
        downloads: [{ label: "Installer", detail: "Windows 10, 11", href: `${ROLLING}/terraverse-launcher-setup.exe`, icon: "simple-icons:windows" }],
    },
    {
        id: "macos",
        label: "macOS",
        icon: "simple-icons:apple",
        downloads: [],
        notReady: "macOS isn't built yet.",
    },
]
