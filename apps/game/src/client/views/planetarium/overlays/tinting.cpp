// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/overlays/tinting.h"
#include "library/world/terragen/src/code/worlds.h"
#include <thread>

namespace planetarium {

Tinting::Tinting(Opts opts) : m_opts(opts)
{
}

std::shared_ptr<const far::Tiles::Tint> Tinting::tintFor(int body)
{
	// what's explored is the tiled body's: the others aren't shown under it
	if (!m_layer || (m_layer->explored && body != m_opts.home))
		return nullptr;
	if (m_layer->image) {
		if (body != m_opts.home)
			return nullptr; // the page's painting is the tiled body's
		auto painter = std::make_shared<Painter>(Painter::Opts{ nullptr, m_layer->overlay, m_layer->image });
		return std::make_shared<const far::Tiles::Tint>([painter](video::SColor own, const far::Tiling::Vec &at) {
			return painter->colour(own, at);
		});
	}
	const std::shared_ptr<const terragen::World> world = worldOf(body);
	if (!world) {
		m_untinted.insert(body);
		return nullptr;
	}
	auto painter = std::make_shared<Painter>(Painter::Opts{ world, m_layer->overlay, nullptr, m_layer->date,
		body == m_opts.home ? m_layer->explored : std::nullopt, m_layer->grid });
	return std::make_shared<const far::Tiles::Tint>([painter](video::SColor own, const far::Tiling::Vec &at) {
		return painter->colour(own, at);
	});
}

float Tinting::contour() const
{
	return m_layer && !m_layer->image && m_layer->overlay == Overlay::Elevation ? 50.f : 0.f;
}

std::vector<int> Tinting::ripe()
{
	std::vector<int> out;
	for (auto it = m_untinted.begin(); it != m_untinted.end();) {
		auto waiting = m_worlds.find(*it);
		if (waiting != m_worlds.end() && waiting->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
			++it;
			continue;
		}
		out.push_back(*it);
		it = m_untinted.erase(it);
	}
	return out;
}

std::shared_ptr<const terragen::World> Tinting::worldOf(int body)
{
	auto found = m_worlds.find(body);
	if (found == m_worlds.end()) {
		const terragen::WorldCode home = terragen::WorldCode::decode(m_opts.horizon.code);
		const terragen::WorldCode code = body == planet::Atlas::HOME ? home
				: terragen::bodyWorlds(home, m_opts.atlas.body(planet::Atlas::HOME).params()).at(body).code;
		// a thread of its own, not std::async: a Planetarium let go mid-build mustn't wait for it
		std::promise<std::shared_ptr<const terragen::World>> built;
		found = m_worlds.emplace(body, built.get_future().share()).first;
		std::thread([code, built = std::move(built)]() mutable {
			try {
				built.set_value(terragen::worldFor(code));
			} catch (...) {
				built.set_exception(std::current_exception());
			}
		}).detach();
	}
	if (found->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return nullptr;
	return found->second.get(); // (rethrows what building it threw)
}

} // namespace planetarium
