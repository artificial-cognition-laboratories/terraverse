// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/core/irrlichttypes.h"

namespace gui { class IGUIEnvironment; class IGUIElement; }

namespace shell {

/*
	SDL only delivers typed text while an Irrlicht element that accepts IME has
	focus. While the page holds the keyboard, an invisible one does.
*/
class Ime
{
public:
	explicit Ime(gui::IGUIEnvironment *guienv);
	~Ime();

	void capture(bool on);

private:
	gui::IGUIEnvironment *m_guienv;
	gui::IGUIElement *m_target;
};

} // namespace shell
