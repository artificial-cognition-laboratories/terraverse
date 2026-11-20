// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/ime.h"
#include <IGUIElement.h>
#include <IGUIEnvironment.h>

namespace shell {

namespace {
class Target : public gui::IGUIElement
{
public:
	explicit Target(gui::IGUIEnvironment *env) :
		IGUIElement(gui::EGUIET_ELEMENT, env, nullptr, -1, core::rect<s32>(0, 0, 0, 0))
	{
	}
	bool acceptsIME() override { return true; }
	void draw() override {}
};
}

Ime::Ime(gui::IGUIEnvironment *guienv) : m_guienv(guienv), m_target(new Target(guienv))
{
}

Ime::~Ime()
{
	m_target->remove();
	m_target->drop();
}

void Ime::capture(bool on)
{
	if (on) {
		// guienv->clear() between menu and game detaches it; we keep our reference.
		if (!m_target->getParent())
			m_guienv->getRootGUIElement()->addChild(m_target);
		m_guienv->setFocus(m_target);
	} else if (m_guienv->getFocus() == m_target) {
		m_guienv->removeFocus(m_target);
	}
}

} // namespace shell
