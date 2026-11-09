// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "options.h"
#include "bridge.h"
#include "exceptions.h"
#include "log.h"
#include "settings.h"
#include <cmath>
#include <vector>

namespace shell {

enum class Kind { Number, Flag, Choice };

struct Option
{
	const char *key;
	Kind kind;
	float min;
	float max;
	const char *choices = nullptr;
};

// Labels and grouping are the page's (ui/src/settings/options.ts); ranges are ours.
static const Option OPTIONS[] = {
	{ "viewing_range", Kind::Number, 20, 1000 },
	{ "fov", Kind::Number, 45, 160 },
	{ "fps_max", Kind::Number, 30, 360 },
	{ "fps_max_unfocused", Kind::Number, 5, 60 },
	{ "undersampling", Kind::Number, 1, 4 },
	{ "display_gamma", Kind::Number, 0.33f, 3.0f },
	{ "fog_start", Kind::Number, 0, 0.99f },
	{ "mouse_sensitivity", Kind::Number, 0.05f, 1.0f },
	{ "sound_volume", Kind::Number, 0, 1 },
	{ "music_volume", Kind::Number, 0, 1 },
	{ "sound_volume_unfocused", Kind::Number, 0, 1 },
	{ "chat_font_size", Kind::Number, 0, 36 },
	{ "recent_chat_messages", Kind::Number, 2, 20 },
	{ "camera_smoothing", Kind::Number, 0, 1 },
	{ "repeat_place_time", Kind::Number, 0.16f, 2.0f },
	{ "joystick_frustum_sensitivity", Kind::Number, 1, 720 },
	{ "joystick_inner_deadzone", Kind::Number, 0, 1 },
	{ "joystick_outer_deadzone", Kind::Number, 0, 1 },
	{ "shadow_map_max_distance", Kind::Number, 10, 1000 },
	{ "shadow_soft_radius", Kind::Number, 1, 15 },
	{ "shadow_strength_gamma", Kind::Number, 0.1f, 10 },
	{ "exposure_compensation", Kind::Number, -1, 1 },
	{ "screenshot_quality", Kind::Number, 0, 100 },
	{ "fullscreen", Kind::Flag, 0, 0 },
	{ "vsync", Kind::Flag, 0, 0 },
	{ "pause_on_lost_focus", Kind::Flag, 0, 0 },
	{ "enable_dynamic_shadows", Kind::Flag, 0, 0 },
	{ "shadow_map_texture_32bit", Kind::Flag, 0, 0 },
	{ "shadow_map_color", Kind::Flag, 0, 0 },
	{ "enable_bloom", Kind::Flag, 0, 0 },
	{ "enable_fog", Kind::Flag, 0, 0 },
	{ "smooth_lighting", Kind::Flag, 0, 0 },
	{ "mip_map", Kind::Flag, 0, 0 },
	{ "bilinear_filter", Kind::Flag, 0, 0 },
	{ "trilinear_filter", Kind::Flag, 0, 0 },
	{ "anisotropic_filter", Kind::Flag, 0, 0 },
	{ "fxaa", Kind::Flag, 0, 0 },
	{ "connected_glass", Kind::Flag, 0, 0 },
	{ "performance_tradeoffs", Kind::Flag, 0, 0 },
	{ "enable_raytraced_culling", Kind::Flag, 0, 0 },
	{ "enable_dig_particles", Kind::Flag, 0, 0 },
	{ "enable_waving_leaves", Kind::Flag, 0, 0 },
	{ "enable_waving_plants", Kind::Flag, 0, 0 },
	{ "enable_post_processing", Kind::Flag, 0, 0 },
	{ "tone_mapping", Kind::Flag, 0, 0 },
	{ "enable_auto_exposure", Kind::Flag, 0, 0 },
	{ "debanding", Kind::Flag, 0, 0 },
	{ "enable_volumetric_lighting", Kind::Flag, 0, 0 },
	{ "enable_translucent_foliage", Kind::Flag, 0, 0 },
	{ "invert_mouse", Kind::Flag, 0, 0 },
	{ "doubletap_jump", Kind::Flag, 0, 0 },
	{ "toggle_sneak_key", Kind::Flag, 0, 0 },
	{ "toggle_aux1_key", Kind::Flag, 0, 0 },
	{ "enable_hotbar_mouse_wheel", Kind::Flag, 0, 0 },
	{ "invert_hotbar_mouse_wheel", Kind::Flag, 0, 0 },
	{ "safe_dig_and_place", Kind::Flag, 0, 0 },
	{ "mute_sound", Kind::Flag, 0, 0 },
	{ "hurt_flash_enabled", Kind::Flag, 0, 0 },
	{ "arm_inertia", Kind::Flag, 0, 0 },
	{ "show_nametag_backgrounds", Kind::Flag, 0, 0 },
	{ "far_view", Kind::Flag, 0, 0 },
	{ "water_fine", Kind::Flag, 0, 0 },
	{ "water_fine_reach", Kind::Number, 4, 24 },
	{ "water_coarse_reach", Kind::Number, 24, 96 },
	{ "ground_detail_near", Kind::Number, 0, 64 },
	{ "ground_detail_far", Kind::Number, 0, 128 },
	{ "far_view_detail", Kind::Number, 1, 4 },
	{ "far_view_max", Kind::Number, 2000, 64000 },
	{ "far_view_debug_levels", Kind::Flag, 0, 0 },
	{ "widget_map", Kind::Flag, 0, 0 },
	{ "widget_map_corner", Kind::Number, 0, 3 },
	{ "widget_map_scale", Kind::Number, 1, 3 },
	{ "widget_map_shape", Kind::Number, 0, 1 },
	{ "widget_map_rotate", Kind::Flag, 0, 0 },
	{ "widget_map_zoom", Kind::Number, 1, 5 },
	{ "widget_world_controls", Kind::Flag, 0, 0 },
	{ "widget_world_controls_corner", Kind::Number, 0, 3 },
	// the world controls: how the body moves when free, what's drawn over the world, its movement numbers
	{ "free_move", Kind::Flag, 0, 0 },
	{ "fast_move", Kind::Flag, 0, 0 },
	{ "noclip", Kind::Flag, 0, 0 },
	{ "view_debug_text", Kind::Flag, 0, 0 },
	{ "view_movement", Kind::Flag, 0, 0 },
	{ "view_profiler", Kind::Flag, 0, 0 },
	{ "view_wireframe", Kind::Flag, 0, 0 },
	{ "view_bboxes", Kind::Flag, 0, 0 },
	{ "view_block_bounds", Kind::Choice, 0, 0, "off,current,near" },
	{ "tune_kick_out", Kind::Number, 0, 12 },
	{ "tune_kick_steer", Kind::Number, 0, 10 },
	{ "tune_wall_run_time", Kind::Number, 0.2f, 4 },
	{ "tune_wall_run_lift", Kind::Number, 0, 6 },
	{ "tune_wall_run_gravity", Kind::Number, 0, 1 },
	{ "tune_shimmy", Kind::Number, 0.2f, 4 },
	{ "tune_hard_landing", Kind::Number, 3, 30 },
	{ "tune_roll_late", Kind::Number, 0, 1 },
	{ "tune_falls_from", Kind::Number, 0.1f, 1.5f },
	{ "tune_falls_at", Kind::Number, 5, 60 },
	{ "antialiasing", Kind::Choice, 0, 0, "none,fsaa,ssaa" },
	{ "fsaa", Kind::Choice, 0, 0, "2,4,8,16" },
	{ "leaves_style", Kind::Choice, 0, 0, "fancy,simple,opaque" },
	{ "shadow_filters", Kind::Choice, 0, 0, "0,1,2" },
	{ "shadow_map_texture_size", Kind::Choice, 0, 0, "1024,2048,4096,8192" },
	{ "occlusion_culler", Kind::Choice, 0, 0, "bfs,loops" },
	{ "node_highlighting", Kind::Choice, 0, 0, "box,halo,none" },
	{ "screenshot_format", Kind::Choice, 0, 0, "png,jpg" },
};

static bool isChoice(const Option &option, const std::string &value)
{
	const std::string choices = option.choices;
	size_t start = 0;
	while (start <= choices.size()) {
		size_t end = choices.find(',', start);
		if (choices.substr(start, end - start) == value)
			return true;
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return false;
}

static const Option &find(const std::string &key)
{
	for (const Option &option : OPTIONS) {
		if (key == option.key)
			return option;
	}
	throw Refused("\"" + key + "\" is not a setting the menu can change");
}

static std::vector<std::string> bindingNames()
{
	std::vector<std::string> names;
	for (const std::string &key : Settings::getLayer(SL_DEFAULTS)->getNames()) {
		if (key.rfind("keymap_", 0) == 0)
			names.push_back(key);
	}
	return names;
}

static void checkBinding(const std::string &key)
{
	for (const std::string &name : bindingNames()) {
		if (name == key)
			return;
	}
	throw Refused("unknown key binding " + key);
}

static std::string keyboardToken(const std::string &value)
{
	size_t start = 0;
	while (start <= value.size()) {
		size_t end = value.find('|', start);
		std::string token = value.substr(start, end == std::string::npos ? end : end - start);
		if (token.rfind("SYSTEM_SCANCODE_", 0) == 0)
			return token;
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return "";
}

static std::string withKeyboard(const std::string &value, const std::string &keyboard)
{
	std::string result = keyboard;
	size_t start = 0;
	while (start < value.size()) {
		size_t end = value.find('|', start);
		std::string token = value.substr(start, end == std::string::npos ? end : end - start);
		if (!token.empty() && token.rfind("SYSTEM_SCANCODE_", 0) != 0) {
			if (!result.empty())
				result += '|';
			result += token;
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return result;
}

static void save()
{
	if (!g_settings_path.empty() && !g_settings->updateConfigFile(g_settings_path.c_str()))
		throw BaseException("shell: could not save settings to " + g_settings_path);
}

Json::Value Options::get() const
{
	Json::Value out(Json::objectValue);
	for (const Option &option : OPTIONS) {
		if (option.kind == Kind::Flag)
			out[option.key] = g_settings->getBool(option.key);
		else if (option.kind == Kind::Number)
			out[option.key] = g_settings->getFloat(option.key);
		else
			out[option.key] = g_settings->get(option.key);
	}
	return out;
}

void Options::set(const std::string &key, const Json::Value &value) const
{
	Json::Value values(Json::objectValue);
	values[key] = value;
	apply(values);
}

void Options::apply(const Json::Value &values) const
{
	if (!values.isObject() || values.empty())
		throw Refused("settings.apply needs a non-empty object");
	const auto keys = values.getMemberNames();
	for (const std::string &key : keys) {
		const Option &option = find(key);
		const Json::Value &value = values[key];
		if (option.kind == Kind::Flag) {
			if (!value.isBool())
				throw Refused(key + " takes true or false");
		} else if (option.kind == Kind::Number) {
			if (!value.isNumeric() || !std::isfinite(value.asDouble()))
				throw Refused(key + " takes a finite number");
			double v = value.asDouble();
			if (v < option.min || v > option.max)
				throw Refused(key + " must be between " + std::to_string(option.min) +
						" and " + std::to_string(option.max));
		} else {
			if (!value.isString() || !isChoice(option, value.asString()))
				throw Refused(key + " is not a supported choice");
		}
	}
	for (const std::string &key : keys) {
		const Option &option = find(key);
		if (option.kind == Kind::Flag)
			g_settings->setBool(key, values[key].asBool());
		else if (option.kind == Kind::Number)
			g_settings->setFloat(key, values[key].asFloat());
		else
			g_settings->set(key, values[key].asString());
	}
	save();
	infostream << "shell: set " << keys.size() << " setting(s)" << std::endl;
}

Json::Value Options::bindings() const
{
	Json::Value out(Json::objectValue);
	for (const std::string &key : bindingNames())
		out[key] = g_settings->get(key);
	return out;
}

void Options::bind(const std::string &key, int scancode, bool replace) const
{
	checkBinding(key);
	// SDL scancodes used by KeyPress. Escape remains the modal's close key.
	if (scancode < 4 || scancode > 231 || scancode == 41)
		throw Refused("that key cannot be assigned");
	const std::string token = "SYSTEM_SCANCODE_" + std::to_string(scancode);
	std::vector<std::string> conflicts;
	for (const std::string &name : bindingNames()) {
		if (name != key && keyboardToken(g_settings->get(name)) == token)
			conflicts.push_back(name);
	}
	if (!conflicts.empty() && !replace)
		throw Refused("key already assigned to " + conflicts.front());
	for (const std::string &name : conflicts)
		g_settings->set(name, withKeyboard(g_settings->get(name), ""));
	g_settings->set(key, withKeyboard(g_settings->get(key), token));
	save();
}

void Options::resetBindings() const
{
	for (const std::string &key : bindingNames())
		g_settings->remove(key);
	save();
}

} // namespace shell
