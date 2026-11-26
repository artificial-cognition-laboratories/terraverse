// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <IEventReceiver.h>
#include <functional>
#include <vector>

namespace video { class IVideoDriver; }

namespace shell {

/*
	The native views drawn between the world and the page, where the page leaves room for them
	(a wiki scene, the inventory's figure): each drawn every frame in the order it came, and
	offered the mouse the page leaves till one takes it. Each draws nothing while it's not shown.
*/
class Underlays
{
public:
	using Draw = std::function<void(video::IVideoDriver *)>;
	using Mouse = std::function<bool(const SEvent &)>;

	// Whose it is: the number to remove it by.
	int add(Draw draw, Mouse mouse);
	void remove(int id);
	bool empty() const { return m_entries.empty(); }
	void draw(video::IVideoDriver *driver) const;
	bool mouse(const SEvent &event) const;

private:
	struct Entry
	{
		int id;
		Draw draw;
		Mouse mouse;
	};
	std::vector<Entry> m_entries;
	int m_next = 1;
};

} // namespace shell
