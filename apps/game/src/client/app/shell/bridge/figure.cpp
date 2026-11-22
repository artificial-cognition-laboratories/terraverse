// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/figure.h"
#include "src/client/app/shell/bridge/scene_rect.h"
#include "src/client/app/shell/host/shell.h"
#include "src/client/app/shell/host/underlays.h"

namespace shell {

static const char *const VERBS[] = { "figure.show", "figure.place", "figure.hide" };

Figures::Figures(Opts o) : m_shell(o.shell)
{
	m_shell.on("figure.show", [this](const Json::Value &intent) {
		m_view.place(sceneRect(intent["rect"]));
		m_view.show();
		return Json::Value();
	});
	m_shell.on("figure.place", [this](const Json::Value &intent) {
		m_view.place(sceneRect(intent["rect"]));
		return Json::Value();
	});
	m_shell.on("figure.hide", [this](const Json::Value &) {
		m_view.hide();
		return Json::Value();
	});
	m_underlay = m_shell.underlays().add([this](video::IVideoDriver *driver) { m_view.draw(driver); },
			[this](const SEvent &event) { return m_view.handle(event); });
}

Figures::~Figures()
{
	m_shell.underlays().remove(m_underlay);
	for (const char *verb : VERBS)
		m_shell.off(verb);
}

} // namespace shell
