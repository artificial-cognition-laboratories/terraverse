// Visible catalog, navigation, and search. Native validation lives in shell/options.cpp.
export type SettingValue = number | boolean | string
type Requirement = { key: string; value: SettingValue; note?: string }
// `applies`: when the engine reads it, if not at once (the render pipeline, shaders and meshes are built on entering a world)
type Base = { key: string; label: string; note: string; terms?: string; requires?: Requirement; applies?: "rejoin" | "restart" }
export type Option =
    | Base & { kind: "slider"; min: number; max: number; step: number; format?: (v: number) => string }
    | Base & { kind: "toggle" }
    | Base & { kind: "choice"; choices: readonly { value: string; label: string }[] }
export type Page = { id: string; title: string; note: string; group: string; options: Option[]; preset?: boolean; bindings?: boolean }
export type Category = { id: string; title: string; pages: Page[] }

const pct = (v: number) => `${Math.round(v * 100)}%`
const blocks = (v: number) => `${v.toLocaleString()} blocks`
const detail = (v: number) => ["Low", "Medium", "High", "Ultra"][Math.round(v) - 1] ?? String(v)
const corner = (v: number) => ["Top left", "Top right", "Bottom left", "Bottom right"][Math.round(v)] ?? String(v)
const yes = (key: string, note?: string): Requirement => ({ key, value: true, note })
const pick = (...items: [string, string][]) => items.map(([value, label]) => ({ value, label }))
const shadows = yes("enable_dynamic_shadows")
const post = yes("enable_post_processing")

export const CATEGORIES: Category[] = [
{ id: "display", title: "Display", pages: [
  { id: "window", title: "Window", group: "Screen", note: "How Terraverse uses your display and behaves in the background.", options: [
    { key: "fullscreen", label: "Fullscreen", note: "Fill the display instead of using a window.", kind: "toggle" },
    { key: "vsync", label: "VSync", note: "Match frames to the display to reduce tearing.", kind: "toggle", applies: "restart" },
    { key: "pause_on_lost_focus", label: "Pause when unfocused", note: "Pause a local world when you switch applications.", kind: "toggle" },
  ] },
  { id: "framerate", title: "Frame rate", group: "Screen", note: "Frame limits and world rendering resolution.", options: [
    { key: "fps_max", label: "Maximum frame rate", note: "The frame rate cap while playing.", kind: "slider", min: 30, max: 360, step: 10, format: v => `${v} fps`, terms: "fps" },
    { key: "fps_max_unfocused", label: "Background frame rate", note: "The cap while Terraverse is out of focus.", kind: "slider", min: 5, max: 60, step: 5, format: v => `${v} fps` },
    { key: "undersampling", label: "World resolution", note: "Render the world lower while keeping the interface sharp.", kind: "slider", min: 1, max: 4, step: 1, format: v => v === 1 ? "100%" : `${Math.round(100 / v)}%`, terms: "render scale", applies: "rejoin" },
  ] },
  { id: "brightness", title: "Brightness", group: "Screen", note: "Calibrate the image for your display.", options: [
    { key: "display_gamma", label: "Brightness curve", note: "Brighten or darken the image mid-tones.", kind: "slider", min: .33, max: 3, step: .05, format: v => v.toFixed(2), terms: "gamma", applies: "rejoin" },
  ] },
  { id: "screenshots", title: "Screenshots", group: "Capture", note: "Choose the format used by the screenshot key.", options: [
    { key: "screenshot_format", label: "File format", note: "PNG is lossless; JPEG creates smaller files.", kind: "choice", choices: pick(["png", "PNG"], ["jpg", "JPEG"]) },
    { key: "screenshot_quality", label: "JPEG quality", note: "Higher quality creates larger files; zero is automatic.", kind: "slider", min: 0, max: 100, step: 5, format: v => v ? `${v}%` : "Automatic", requires: { key: "screenshot_format", value: "jpg", note: "Available for JPEG screenshots." } },
  ] },
] },
{ id: "graphics", title: "Graphics", pages: [
  { id: "presets", title: "Presets", group: "Quality", note: "Start with a graphics profile, then tune individual settings.", preset: true, options: [] },
  { id: "performance", title: "Performance", group: "Quality", note: "Compatibility controls for difficult hardware.", options: [
    { key: "performance_tradeoffs", label: "Performance trade-offs", note: "Allow minor visual glitches to reduce work.", kind: "toggle", applies: "rejoin" },
    { key: "occlusion_culler", label: "Occlusion culling", note: "Modern is preferred; compatible is a fallback.", kind: "choice", choices: pick(["bfs", "Modern"], ["loops", "Compatible"]) },
    { key: "enable_raytraced_culling", label: "Ray-traced culling", note: "Avoid drawing terrain hidden by terrain.", kind: "toggle" },
  ] },
  { id: "distance", title: "View distance", group: "World", note: "How far the world is drawn, and the detail of the land beyond loaded blocks.", options: [
    { key: "viewing_range", label: "Render distance", note: "Blocks rendered at full detail around you.", kind: "slider", min: 20, max: 1000, step: 10, format: blocks },
    { key: "far_view", label: "Distant terrain", note: "Draw land beyond loaded blocks.", kind: "toggle", terms: "horizon far lod" },
    { key: "far_view_detail", label: "Distant terrain detail", note: "How soon distant tiles split into finer ones.", kind: "slider", min: 1, max: 4, step: 1, format: detail, terms: "lod horizon", requires: yes("far_view") },
    { key: "far_view_max", label: "Furthest view", note: "How far peaks show past the horizon.", kind: "slider", min: 2000, max: 64000, step: 1000, format: blocks, requires: yes("far_view") },
    { key: "far_view_debug_levels", label: "Show distant tile levels", note: "Debug: tint each distant tile by its detail level; stand-ins darker.", kind: "toggle", terms: "lod debug", requires: yes("far_view") },
  ] },
  { id: "terrain", title: "Terrain detail", group: "World", note: "How far cut and smoothed ground keeps its fine shape before it's drawn coarser.", options: [
    { key: "ground_detail_near", label: "Sixteenth detail", note: "Ground is drawn in sixteenths of a block within this distance.", kind: "slider", min: 0, max: 64, step: 4, format: blocks, terms: "lod", applies: "rejoin" },
    { key: "ground_detail_far", label: "Quarter detail", note: "Then in quarters out to this distance, and whole blocks past it.", kind: "slider", min: 0, max: 128, step: 4, format: blocks, terms: "lod", applies: "rejoin" },
  ] },
  { id: "water", title: "Water", group: "World", note: "Water near you is simulated in columns, so it sloshes and carries wakes. Further out it's drawn flat.", options: [
    { key: "water_fine", label: "Simulated water", note: "Off draws all water flat, which is cheapest.", kind: "toggle", terms: "fine waves lod", applies: "rejoin" },
    { key: "water_fine_reach", label: "Fine water reach", note: "Simulated in sixteenths of a block within this distance.", kind: "slider", min: 4, max: 24, step: 2, format: blocks, terms: "lod", requires: yes("water_fine"), applies: "rejoin" },
    { key: "water_coarse_reach", label: "Coarse water reach", note: "Simulated in quarters out to this distance, then flat.", kind: "slider", min: 24, max: 96, step: 8, format: blocks, terms: "lod", requires: yes("water_fine"), applies: "rejoin" },
  ] },
  { id: "fog", title: "Fog", group: "World", note: "How the edge of the loaded world meets the air.", options: [
    { key: "enable_fog", label: "Fog", note: "Blend the edge of the world into the atmosphere.", kind: "toggle" },
    { key: "fog_start", label: "Fog start", note: "How far into the visible range fog begins.", kind: "slider", min: 0, max: .99, step: .01, format: pct, requires: yes("enable_fog") },
  ] },
  { id: "lighting", title: "Lighting", group: "World", note: "How light falls on the world and how the camera adapts to it.", options: [
    { key: "smooth_lighting", label: "Smooth lighting", note: "Blend light across blocks and add ambient occlusion.", kind: "toggle", applies: "rejoin" },
    { key: "enable_volumetric_lighting", label: "Volumetric light", note: "Draw shafts of light through the atmosphere.", kind: "toggle", requires: yes("enable_bloom"), applies: "rejoin" },
    { key: "enable_auto_exposure", label: "Automatic exposure", note: "Adapt brightness between dark and bright places.", kind: "toggle", requires: post, applies: "rejoin" },
    { key: "exposure_compensation", label: "Exposure bias", note: "Favour a brighter or darker image.", kind: "slider", min: -1, max: 1, step: .1, format: v => `${v > 0 ? "+" : ""}${v.toFixed(1)} EV`, requires: yes("enable_auto_exposure") },
  ] },
  { id: "shadows", title: "Shadows", group: "World", note: "Real-time shadows from the sun.", options: [
    { key: "enable_dynamic_shadows", label: "Dynamic shadows", note: "Cast real-time shadows from the sun.", kind: "toggle", applies: "rejoin" },
    { key: "shadow_map_max_distance", label: "Shadow distance", note: "How far dynamic shadows are drawn.", kind: "slider", min: 10, max: 1000, step: 10, format: blocks, requires: shadows, applies: "rejoin" },
    { key: "shadow_map_texture_size", label: "Shadow resolution", note: "Higher resolution makes distant shadow edges clearer.", kind: "choice", choices: pick(["1024", "Low"], ["2048", "Medium"], ["4096", "High"], ["8192", "Ultra"]), requires: shadows, applies: "rejoin" },
    { key: "shadow_filters", label: "Shadow filtering", note: "Smoother edges cost more graphics performance.", kind: "choice", choices: pick(["0", "Sharp"], ["1", "Soft"], ["2", "Softer"]), requires: shadows, applies: "rejoin" },
    { key: "shadow_soft_radius", label: "Shadow softness", note: "Increase the apparent sun size for softer edges.", kind: "slider", min: 1, max: 15, step: .5, requires: shadows, applies: "rejoin" },
    { key: "shadow_strength_gamma", label: "Shadow strength", note: "Higher values make shadows darker.", kind: "slider", min: .1, max: 3, step: .1, requires: shadows, applies: "rejoin" },
    { key: "shadow_map_texture_32bit", label: "High precision shadows", note: "Reduce artefacts at a memory cost.", kind: "toggle", requires: shadows, applies: "rejoin" },
    { key: "shadow_map_color", label: "Coloured shadows", note: "Let translucent blocks tint their shadows.", kind: "toggle", requires: shadows, applies: "rejoin" },
  ] },
  { id: "plants", title: "Plants", group: "World", note: "How leaves and plants are drawn and move.", options: [
    { key: "leaves_style", label: "Leaves", note: "Fancy draws every face; opaque is fastest.", kind: "choice", choices: pick(["opaque", "Opaque"], ["simple", "Simple"], ["fancy", "Fancy"]), applies: "rejoin" },
    { key: "enable_waving_leaves", label: "Moving leaves", note: "Animate leaves in the wind.", kind: "toggle", applies: "rejoin" },
    { key: "enable_waving_plants", label: "Moving plants", note: "Animate grasses and plants.", kind: "toggle", applies: "rejoin" },
    { key: "enable_translucent_foliage", label: "Sunlit foliage", note: "Simulate sunlight passing through leaves.", kind: "toggle", requires: shadows, applies: "rejoin" },
  ] },
  { id: "textures", title: "Textures", group: "Image", note: "Texture clarity at a distance and at an angle.", options: [
    { key: "mip_map", label: "Mipmapping", note: "Keep distant textures stable.", kind: "toggle", applies: "rejoin" },
    { key: "bilinear_filter", label: "Bilinear filtering", note: "Smooth textures when scaled.", kind: "toggle", applies: "rejoin" },
    { key: "trilinear_filter", label: "Trilinear filtering", note: "Smooth transitions between detail levels.", kind: "toggle", requires: yes("mip_map"), applies: "rejoin" },
    { key: "anisotropic_filter", label: "Anisotropic filtering", note: "Keep angled surfaces clear.", kind: "toggle", requires: yes("mip_map"), applies: "rejoin" },
    { key: "connected_glass", label: "Connected glass", note: "Hide shared faces between compatible glass.", kind: "toggle", applies: "rejoin" },
  ] },
  { id: "edges", title: "Edge smoothing", group: "Image", note: "Smooth the silhouettes of blocks and objects.", options: [
    { key: "antialiasing", label: "Edge smoothing", note: "MSAA is cheaper; SSAA draws the whole world larger.", kind: "choice", choices: pick(["none", "Off"], ["fsaa", "MSAA"], ["ssaa", "SSAA"]), terms: "antialiasing", applies: "rejoin" },
    { key: "fsaa", label: "Smoothing samples", note: "More samples improve edges at a performance cost.", kind: "choice", choices: pick(["2", "2×"], ["4", "4×"], ["8", "8×"], ["16", "16×"]), requires: { key: "antialiasing", value: "fsaa" }, applies: "rejoin" },
    { key: "fxaa", label: "Fast edge smoothing", note: "Apply a fast post-processing edge pass.", kind: "toggle", requires: post, applies: "rejoin" },
  ] },
  { id: "post", title: "Post-processing", group: "Image", note: "Camera-like effects applied after rendering the world.", options: [
    { key: "enable_post_processing", label: "Post-processing", note: "Enable the effects pipeline.", kind: "toggle", applies: "rejoin" },
    { key: "enable_bloom", label: "Bloom", note: "Add glow around bright light.", kind: "toggle", requires: post, applies: "rejoin" },
    { key: "tone_mapping", label: "Filmic tone mapping", note: "Compress highlights and deepen mid-range contrast.", kind: "toggle", requires: post, applies: "rejoin" },
    { key: "debanding", label: "Debanding", note: "Dither gradients to reduce visible bands.", kind: "toggle", requires: post, applies: "rejoin" },
  ] },
] },
{ id: "controls", title: "Controls", pages: [
  { id: "keybindings", title: "Keybindings", group: "Input", note: "Assign keys for movement, actions, and screens.", bindings: true, options: [] },
  { id: "camera", title: "Mouse & camera", group: "Input", note: "How the view responds to input.", options: [
    { key: "mouse_sensitivity", label: "Mouse sensitivity", note: "How quickly the camera turns.", kind: "slider", min: .05, max: 1, step: .01, format: pct },
    { key: "invert_mouse", label: "Invert vertical look", note: "Move the mouse down to look up.", kind: "toggle" },
    { key: "fov", label: "Field of view", note: "The width of your first-person view.", kind: "slider", min: 45, max: 160, step: 1, format: v => `${v}°`, terms: "camera view" },
    { key: "camera_smoothing", label: "Camera smoothing", note: "Ease camera turns instead of following immediately.", kind: "slider", min: 0, max: 1, step: .05, format: pct },
    { key: "arm_inertia", label: "Hand inertia", note: "Let held items lag slightly behind camera movement.", kind: "toggle" },
  ] },
  { id: "controller", title: "Controller", group: "Input", note: "Tune analogue camera movement and stick dead zones.", options: [
    { key: "joystick_frustum_sensitivity", label: "Look sensitivity", note: "Controller camera rotation speed.", kind: "slider", min: 1, max: 720, step: 5, format: v => `${v}°/s` },
    { key: "joystick_inner_deadzone", label: "Inner dead zone", note: "Ignore small movement around stick centre.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
    { key: "joystick_outer_deadzone", label: "Outer dead zone", note: "Reach maximum input before the stick edge.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  ] },
  { id: "movement", title: "Movement", group: "Gameplay", note: "Choose how movement shortcuts behave.", options: [
    { key: "doubletap_jump", label: "Double-tap to fly", note: "Double-tap jump to fly where allowed.", kind: "toggle" },
    { key: "toggle_sneak_key", label: "Toggle crouch", note: "Press once to crouch until pressed again.", kind: "toggle" },
    { key: "toggle_aux1_key", label: "Toggle sprint", note: "Press once to sprint until pressed again.", kind: "toggle" },
  ] },
  { id: "interaction", title: "Interaction", group: "Gameplay", note: "Mouse wheel and held-button behaviour.", options: [
    { key: "enable_hotbar_mouse_wheel", label: "Wheel selects hotbar", note: "Use the mouse wheel for hotbar slots.", kind: "toggle" },
    { key: "invert_hotbar_mouse_wheel", label: "Invert hotbar wheel", note: "Reverse hotbar selection direction.", kind: "toggle", requires: yes("enable_hotbar_mouse_wheel") },
    { key: "safe_dig_and_place", label: "Safe dig and place", note: "Prevent repeated actions while holding a button.", kind: "toggle" },
    { key: "repeat_place_time", label: "Placement repeat delay", note: "Time between held-button placements.", kind: "slider", min: .16, max: 2, step: .02, format: v => `${v.toFixed(2)} s`, requires: { key: "safe_dig_and_place", value: false } },
  ] },
] },
{ id: "audio", title: "Audio", pages: [{ id: "levels", title: "Volume", group: "Sound", note: "Sound and music levels for active and background play.", options: [
  { key: "sound_volume", label: "Master volume", note: "Overall game sound level.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  { key: "music_volume", label: "Music volume", note: "Menu and future in-world music level.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  { key: "sound_volume_unfocused", label: "Background volume", note: "Sound multiplier while out of focus.", kind: "slider", min: 0, max: 1, step: .01, format: pct },
  { key: "mute_sound", label: "Mute all sound", note: "Turn off all game audio.", kind: "toggle" },
] }] },
{ id: "interface", title: "Interface", pages: [
  { id: "map", title: "Map", group: "HUD", note: "A terrain map centred on you in Terraverse worlds.", options: [
    { key: "widget_map", label: "Show map", note: "Show terrain around you.", kind: "toggle" },
    { key: "widget_map_corner", label: "Map position", note: "Choose the corner where it sits.", kind: "slider", min: 0, max: 3, step: 1, format: corner, requires: yes("widget_map") },
    { key: "widget_map_scale", label: "Map size", note: "Choose its size on screen.", kind: "slider", min: 1, max: 3, step: 1, format: v => ["", "Small", "Medium", "Large"][Math.round(v)]!, requires: yes("widget_map") },
    { key: "widget_map_shape", label: "Map shape", note: "Use a square or circular frame.", kind: "slider", min: 0, max: 1, step: 1, format: v => v ? "Circle" : "Square", requires: yes("widget_map") },
    { key: "widget_map_rotate", label: "Follow heading", note: "Keep your direction at the top.", kind: "toggle", requires: yes("widget_map") },
    { key: "widget_map_zoom", label: "Map zoom", note: "Choose how much ground fits.", kind: "slider", min: 1, max: 5, step: 1, format: v => ["", "Far", "Wide", "Normal", "Close", "Near"][Math.round(v)]!, requires: yes("widget_map") },
  ] },
  { id: "world-controls", title: "World controls", group: "HUD", note: "Build and time controls when permitted.", options: [
    { key: "widget_world_controls", label: "Show world controls", note: "Show controls while paused.", kind: "toggle" },
    { key: "widget_world_controls_corner", label: "Controls position", note: "Choose the corner where they sit.", kind: "slider", min: 0, max: 3, step: 1, format: corner, requires: yes("widget_world_controls") },
  ] },
  { id: "targeting", title: "Targeting", group: "HUD", note: "How what you're pointing at and other players are marked.", options: [
    { key: "node_highlighting", label: "Target highlight", note: "How the block under the crosshair is identified.", kind: "choice", choices: pick(["box", "Outline"], ["halo", "Halo"], ["none", "Off"]) },
    { key: "show_nametag_backgrounds", label: "Nametag backgrounds", note: "Back player names for contrast.", kind: "toggle" },
  ] },
  { id: "chat", title: "Chat", group: "Other", note: "Size and history of the chat log.", options: [
    { key: "chat_font_size", label: "Chat text size", note: "Zero follows the interface font size.", kind: "slider", min: 0, max: 36, step: 1, format: v => v ? `${v} pt` : "Automatic", applies: "rejoin" },
    { key: "recent_chat_messages", label: "Recent messages", note: "Messages retained on the HUD.", kind: "slider", min: 2, max: 20, step: 1, applies: "rejoin" },
  ] },
  { id: "feedback", title: "Feedback", group: "Other", note: "Moment-to-moment effects you may want to turn down.", options: [
    { key: "hurt_flash_enabled", label: "Damage flash", note: "Flash the screen when you take damage.", kind: "toggle", terms: "flashing accessibility" },
    { key: "enable_dig_particles", label: "Digging particles", note: "Show fragments when blocks are dug.", kind: "toggle" },
  ] },
] },
]

export const GRAPHICS_PRESETS = [
  { id: "performance", label: "Performance", note: "Short view, flat water, minimal effects.", values: { viewing_range: 100, far_view: false, far_view_detail: 1, far_view_max: 2000, water_fine: false, water_fine_reach: 4, water_coarse_reach: 24, ground_detail_near: 4, ground_detail_far: 12, enable_dynamic_shadows: false, enable_bloom: false, enable_fog: true, smooth_lighting: false, mip_map: false, anisotropic_filter: false, antialiasing: "none", fxaa: false, leaves_style: "opaque", enable_waving_leaves: false, enable_waving_plants: false } },
  { id: "balanced", label: "Balanced", note: "Clear distance, modest detail.", values: { viewing_range: 200, far_view: true, far_view_detail: 2, far_view_max: 16000, water_fine: true, water_fine_reach: 10, water_coarse_reach: 48, ground_detail_near: 12, ground_detail_far: 32, enable_dynamic_shadows: false, enable_bloom: false, enable_fog: true, smooth_lighting: true, mip_map: true, anisotropic_filter: true, antialiasing: "none", fxaa: true, leaves_style: "simple", enable_waving_leaves: false, enable_waving_plants: false } },
  { id: "high", label: "High", note: "Long views and full lighting.", values: { viewing_range: 350, far_view: true, far_view_detail: 3, far_view_max: 32000, water_fine: true, water_fine_reach: 16, water_coarse_reach: 64, ground_detail_near: 16, ground_detail_far: 48, enable_dynamic_shadows: true, enable_bloom: true, enable_fog: true, smooth_lighting: true, mip_map: true, anisotropic_filter: true, antialiasing: "fsaa", fsaa: "2", fxaa: true, leaves_style: "fancy", enable_waving_leaves: true, enable_waving_plants: true } },
  { id: "ultra", label: "Ultra", note: "Maximum horizon and detail.", values: { viewing_range: 600, far_view: true, far_view_detail: 4, far_view_max: 64000, water_fine: true, water_fine_reach: 24, water_coarse_reach: 96, ground_detail_near: 24, ground_detail_far: 64, enable_dynamic_shadows: true, enable_bloom: true, enable_fog: true, smooth_lighting: true, mip_map: true, anisotropic_filter: true, antialiasing: "fsaa", fsaa: "4", fxaa: true, leaves_style: "fancy", enable_waving_leaves: true, enable_waving_plants: true } },
] as const
