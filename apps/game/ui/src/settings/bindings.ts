// Keyboard shortcuts shown in Settings. Gamepad and mouse parts of each
// engine binding are retained when the keyboard part changes.
export const BINDING_GROUPS = [
    { title: "Movement", bindings: [
        ["keymap_forward", "Move forward"], ["keymap_backward", "Move backward"],
        ["keymap_left", "Move left"], ["keymap_right", "Move right"],
        ["keymap_jump", "Jump"], ["keymap_sneak", "Crouch (tap or hold) / fly down"], ["keymap_prone", "Lie down"],
        ["keymap_aux1", "Run / thrust"], ["keymap_freemove", "Toggle flight"],
        ["keymap_brake", "Brake (thrusting)"],
    ] },
    { title: "Actions", bindings: [
        ["keymap_dig", "Dig / attack"], ["keymap_place", "Place / use"],
        ["keymap_inventory", "Inventory"], ["keymap_drop", "Drop item"],
        ["keymap_zoom", "Zoom"], ["keymap_chat", "Chat"],
        ["keymap_pour", "Pour what's in hand"],
        ["keymap_hotbar_next", "Next hotbar slot"], ["keymap_hotbar_previous", "Previous hotbar slot"],
        ["keymap_autoforward", "Automatic forward"],
    ] },
    { title: "Building", bindings: [
        ["keymap_tools", "Build mode"], ["keymap_cursor", "Free the cursor"],
        ["keymap_focus", "Work on an object"], ["keymap_turn_left", "Turn what's carried left"],
        ["keymap_turn_right", "Turn what's carried right"], ["keymap_tool_next", "Next tool (Add, Cut, Paint) / tilt what's carried"],
        ["keymap_undo", "Undo"], ["keymap_mirror", "Mirror"],
        ["keymap_copy", "Ctrl+: copy a part"], ["keymap_paste", "Ctrl+: paste"],
        ["keymap_save_part", "Ctrl+: save a part to the library"],
        ["keymap_alt", "Alt: add into the world, 1–9 hotbar while building, = mirror, click carries"],
    ] },
    { title: "Screens", bindings: [
        ["keymap_lab", "Lab"], ["keymap_wiki", "Wiki"], ["keymap_planetarium", "Planetarium"],
        ["keymap_minimap", "Map"], ["keymap_toggle_hud", "Toggle HUD"],
        ["keymap_camera_mode", "Camera mode"],
    ] },
    { title: "System", bindings: [
        ["keymap_fullscreen", "Fullscreen"], ["keymap_screenshot", "Screenshot"],
        ["keymap_mute", "Mute sound"], ["keymap_increase_volume", "Increase volume"],
        ["keymap_decrease_volume", "Decrease volume"],
        ["keymap_increase_viewing_range_min", "Increase view distance"],
        ["keymap_decrease_viewing_range_min", "Decrease view distance"],
    ] },
] as const

export const BINDINGS = BINDING_GROUPS.flatMap((g) => g.bindings.map(([key, label]) => ({ key, label, group: g.title })))

// SDL scancodes. The page captures KeyboardEvent.code, which describes a
// physical key and is stable across keyboard layouts like the engine's code.
const SPECIAL: Record<string, number> = {
    Enter: 40, Escape: 41, Backspace: 42, Tab: 43, Space: 44,
    Minus: 45, Equal: 46, BracketLeft: 47, BracketRight: 48,
    Backslash: 49, Semicolon: 51, Quote: 52, Backquote: 53,
    Comma: 54, Period: 55, Slash: 56, CapsLock: 57,
    PrintScreen: 70, ScrollLock: 71, Pause: 72, Insert: 73,
    Home: 74, PageUp: 75, Delete: 76, End: 77, PageDown: 78,
    ArrowRight: 79, ArrowLeft: 80, ArrowDown: 81, ArrowUp: 82,
    ControlLeft: 224, ShiftLeft: 225, AltLeft: 226,
    ControlRight: 228, ShiftRight: 229, AltRight: 230,
}
for (let i = 1; i <= 12; i++) SPECIAL[`F${i}`] = 57 + i
for (let i = 0; i < 26; i++) SPECIAL[`Key${String.fromCharCode(65 + i)}`] = 4 + i
for (let i = 1; i <= 9; i++) SPECIAL[`Digit${i}`] = 29 + i
SPECIAL.Digit0 = 39
const LABELS: Record<string, string> = {
    Space: "Space", ShiftLeft: "Left Shift", ShiftRight: "Right Shift",
    ControlLeft: "Left Ctrl", ControlRight: "Right Ctrl",
    AltLeft: "Left Alt", AltRight: "Right Alt",
    ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right",
    Backquote: "`", BracketLeft: "[", BracketRight: "]",
    Slash: "/", Backslash: "\\", Period: ".", Comma: ",",
    Semicolon: ";", Quote: "'", Minus: "-", Equal: "+",
}
const CODE_BY_SCANCODE = new Map(Object.entries(SPECIAL).map(([code, number]) => [number, code]))
export function scancodeFor(code: string) { return SPECIAL[code] ?? null }
export function bindingScancode(value: string) {
    const token = value.split("|").find((part) => part.startsWith("SYSTEM_SCANCODE_"))
    return token ? Number(token.slice("SYSTEM_SCANCODE_".length)) : null
}
export function bindingLabel(value: string) {
    const scancode = bindingScancode(value)
    if (scancode === null) return "Unbound"
    const code = CODE_BY_SCANCODE.get(scancode)
    if (!code) return `Key ${scancode}`
    if (code.startsWith("Key")) return code.slice(3)
    if (code.startsWith("Digit")) return code.slice(5)
    return LABELS[code] ?? code
}
