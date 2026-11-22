// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/underlays.h"
#include "src/shared/base/exceptions.h"
#include <algorithm>

namespace shell {

int Underlays::add(Draw draw, Mouse mouse)
{
	m_entries.push_back({ m_next, std::move(draw), std::move(mouse) });
	return m_next++;
}

void Underlays::remove(int id)
{
	const auto found = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return e.id == id; });
	if (found == m_entries.end())
		throw BaseException("shell: no underlay " + std::to_string(id));
	m_entries.erase(found);
}

void Underlays::draw(video::IVideoDriver *driver) const
{
	for (const Entry &e : m_entries)
		e.draw(driver);
}

bool Underlays::mouse(const SEvent &event) const
{
	for (const Entry &e : m_entries)
		if (e.mouse(event))
			return true;
	return false;
}

} // namespace shell
