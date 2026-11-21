// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "src/client/app/gameui.h"
#include "src/client/world/player/body.h" // Terra Firma
#include "src/client/render/fonts/static_text.h"
#include "src/shared/platform/system/gettext.h"
#include "src/client/app/menus/mainmenumanager.h"
#include "src/client/app/menus/guiChatConsole.h"
#include "src/client/views/game/hud/statusTextHelper.h"
#include "src/shared/util/text/enriched_string.h"
#include "src/shared/game/pointing/pointedthing.h"
#include "src/client/world/session/client.h"
#include "src/client/render/map/clientmap.h"
#include "src/client/render/fonts/fontengine.h"
#include "src/shared/game/objects/hud_element.h" // HUD_FLAG_*
#include "src/shared/game/content/nodedef.h"
#include "src/client/world/objects/localplayer.h"
#include "src/shared/platform/system/profiler.h"
#include "src/client/render/renderingengine.h"
#include "src/shared/base/version.h"
#include <IGUIFont.h>

inline static const char *yawToDirectionString(int yaw)
{
	static const char *direction[4] =
		{"North +Z", "West -X", "South -Z", "East +X"};

	yaw = wrapDegrees_0_360(yaw);
	yaw = (yaw + 45) % 360 / 90;

	return direction[yaw];
}

void GameUI::init()
{
	// First line of debug text
	m_guitext = gui::StaticText::add(guienv, utf8_to_wide(PROJECT_NAME_C).c_str(),
		core::recti(), false, true, guiroot);

	// Second line of debug text
	m_guitext2 = gui::StaticText::add(guienv, L"", core::recti(), false,
		true, guiroot);

	// Terra Firma: no chat list over the game: Luanti's, in the corner, showed
	// every join and log line. Chat comes back as part of the UI.

	// Infotext of nodes and objects.
	// If in debug mode, object debug infos shown here, too.
	m_guitext_info = gui::StaticText::add(guienv, L"",
		// Size is limited; text will be truncated after 6 lines.
		core::rect<s32>(0, 0, 400, g_fontengine->getTextHeight() * 6) +
			v2s32(100, g_fontengine->getTextHeight() * 3),
			false, true, guiroot);

	// Status message for in-game notifications (fly/fast mode, volume changes, etc.)
	m_status_text = std::make_unique<StatusTextHelper>(guienv, guiroot);
	m_status_text->setGameStyle();

	// Profiler text (size is updated when text is updated)
	m_guitext_profiler = gui::StaticText::add(guienv, L"<Profiler>",
		core::recti(), false, false, guiroot);
	m_guitext_profiler->setOverrideFont(g_fontengine->getFont(
		g_fontengine->getDefaultFontSize() * 0.9f, FM_Mono));
	m_guitext_profiler->setVisible(false);
}

void GameUI::update(const RunStats &stats, Client *client, MapDrawControl *draw_control,
	const CameraOrientation &cam, const PointedThing &pointed_old,
	const GUIChatConsole *chat_console, float dtime)
{
	v2u32 screensize = RenderingEngine::getWindowSize();

	LocalPlayer *player = client->getEnv().getLocalPlayer();

	s32 minimal_debug_height = 0;

	// Minimal debug text must only contain info that can't give a gameplay advantage
	if (m_flags.show_minimal_debug) {
		const u16 fps = 1.0f / stats.dtime_jitter.avg;
		m_drawtime_avg *= 0.95f;
		m_drawtime_avg += 0.05f * (stats.drawtime / 1000);

		std::ostringstream os(std::ios_base::binary);
		os << std::fixed
			<< PROJECT_NAME_C " " << g_version_hash
			<< " | FPS: " << fps
			<< std::setprecision(m_drawtime_avg < 10 ? 1 : 0)
			<< " | drawtime: " << m_drawtime_avg << "ms"
			<< std::setprecision(1)
			<< " | dtime jitter: "
			<< (stats.dtime_jitter.max_fraction * 100.0f) << "%"
			<< std::setprecision(1)
			<< " | view range: "
			<< (draw_control->range_all ? "All" : itos(draw_control->wanted_range))
			<< std::setprecision(2)
			<< " | RTT: " << (client->getRTT() * 1000.0f) << "ms";

		m_guitext->setRelativePosition(core::rect<s32>(5, 5, screensize.X, screensize.Y));

		setStaticText(m_guitext, utf8_to_wide(os.str()));

		minimal_debug_height = m_guitext->getTextHeight();
	}

	// Finally set the guitext visible depending on the flag
	m_guitext->setVisible(m_flags.show_minimal_debug);

	// Basic debug text also shows info that might give a gameplay advantage
	if (m_flags.show_basic_debug) {
		v3f player_position = player->getPosition();

		std::ostringstream os(std::ios_base::binary);
		os << std::setprecision(1) << std::fixed
			<< "pos: (" << (player_position.X / BS)
			<< ", " << (player_position.Y / BS)
			<< ", " << (player_position.Z / BS)
			<< ") | yaw: " << (wrapDegrees_0_360(cam.camera_yaw)) << "° "
			<< yawToDirectionString(cam.camera_yaw)
			<< " | pitch: " << (-wrapDegrees_180(cam.camera_pitch)) << "°"
			<< " | seed: " << ((u64)client->getMapSeed());

		if (pointed_old.type == POINTEDTHING_NODE) {
			ClientMap &map = client->getEnv().getClientMap();
			const NodeDefManager *nodedef = client->getNodeDefManager();
			MapNode n = map.getNode(pointed_old.node_undersurface);

			if (n.getContent() != CONTENT_IGNORE) {
				if (nodedef->get(n).name == "unknown") {
					os << ", pointed: <unknown node>";
				} else {
					os << ", pointed: " << nodedef->get(n).name;
				}
				os << ", param2: " << (u64) n.getParam2();
			}
		}

		// Terra Firma: the body, for tuning its movement (world controls, View)
		if (g_settings->getBool("view_movement"))
			os << "\n" << client->getBody().readout();

		// Terra Firma: the orbit the player's on (sprint 59; a stand-in for the flight HUD)
		if (const std::optional<orbit::Elements> e = client->getBody().orbit()) {
			os << "\n" << (client->getBody().thrusting() ? "thrusting | " : "")
				<< "speed: " << e->speed << " m/s | altitude: " << e->altitude << " m | ";
			if (e->bound)
				os << "apoapsis: " << e->apoapsis << " m | periapsis: " << e->periapsis
					<< " m | period: " << e->period << " s";
			else
				os << "escaping | periapsis: " << e->periapsis << " m";
		}

		m_guitext2->setRelativePosition(core::rect<s32>(5, 5 + minimal_debug_height,
				screensize.X, screensize.Y));

		setStaticText(m_guitext2, utf8_to_wide(os.str()).c_str());
	}

	m_guitext2->setVisible(m_flags.show_basic_debug);

	setStaticText(m_guitext_info, m_infotext.c_str());
	m_guitext_info->setVisible(m_flags.show_hud && g_menumgr.menuCount() == 0);

	// Update status message element
	if (m_status_text)
		m_status_text->update(dtime);
}

void GameUI::initFlags()
{
	m_flags = GameUI::Flags();
}

void GameUI::showTranslatedStatusText(const char *str)
{
	showStatusText(wstrgettext(str));
}

void GameUI::updateProfiler()
{
	m_guitext_profiler->setVisible(m_profiler_current_page != 0);
	if (m_profiler_current_page == 0)
		return;

	std::ostringstream oss(std::ios_base::binary);
	oss << "Profiler page " << (int)m_profiler_current_page
		<< "/" << (int)m_profiler_max_page
		<< ", elapsed: " << g_profiler->getElapsedMs() << " ms" << std::endl;
	g_profiler->print(oss, m_profiler_current_page, m_profiler_max_page);

	EnrichedString str(utf8_to_wide(oss.str()));
	str.setBackground(video::SColor(120, 0, 0, 0));
	setStaticText(m_guitext_profiler, str);

	v2s32 upper_left(5, 10);
	if (m_flags.show_minimal_debug)
		upper_left.Y += m_guitext->getTextHeight();
	if (m_flags.show_basic_debug)
		upper_left.Y += m_guitext2->getTextHeight();

	v2s32 lower_right = upper_left;
	lower_right.X += m_guitext_profiler->getTextWidth() + 5;
	lower_right.Y += m_guitext_profiler->getTextHeight();

	m_guitext_profiler->setRelativePosition(core::recti(upper_left, lower_right));

	// Really dumb heuristic (we have a fixed number of pages, not a fixed page size)
	const v2u32 window_size = RenderingEngine::getWindowSize();
	if (upper_left.Y + m_guitext_profiler->getTextHeight()
		> window_size.Y * 0.7f) {
		if (m_profiler_max_page < 5) {
			m_profiler_max_page++;
			updateProfiler(); // do it again
		}
	}
}

void GameUI::toggleHud()
{
	m_flags.show_hud = !m_flags.show_hud;
	if (m_flags.show_hud)
		showTranslatedStatusText("HUD shown");
	else
		showTranslatedStatusText("HUD hidden");
}

void GameUI::toggleProfiler()
{
	m_profiler_current_page = (m_profiler_current_page + 1) % (m_profiler_max_page + 1);

	// FIXME: This updates the profiler with incomplete values
	updateProfiler();

	if (m_profiler_current_page != 0) {
		std::wstring msg = fwgettext("Profiler shown (page %d of %d)",
				m_profiler_current_page, m_profiler_max_page);
		showStatusText(msg);
	} else {
		showTranslatedStatusText("Profiler hidden");
	}
}

void GameUI::clearText()
{
	if (m_guitext) {
		m_guitext->remove();
		m_guitext = nullptr;
	}

	if (m_guitext2) {
		m_guitext2->remove();
		m_guitext2 = nullptr;
	}

	if (m_guitext_info) {
		m_guitext_info->remove();
		m_guitext_info = nullptr;
	}

	m_status_text.reset();

	if (m_guitext_profiler) {
		m_guitext_profiler->remove();
		m_guitext_profiler = nullptr;
	}
}
