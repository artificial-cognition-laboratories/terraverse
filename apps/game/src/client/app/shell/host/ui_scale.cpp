// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/ui_scale.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace shell {

double UiScale::zoomLevel(float scale)
{
	return std::log((double)scale) / std::log(1.2);
}

namespace {
float clamped(float chosen);
} // namespace

UiScale::UiScale(Opts o) : density(o.density), chosen(clamped(o.chosen)) {}

float UiScale::of(unsigned width, unsigned height) const
{
	// the window in CSS pixels at the display's density, then how far past the reference it is
	const float fit = std::min(width / density / REFERENCE_WIDTH, height / density / REFERENCE_HEIGHT);
	const float scale = density * std::max(1.0f, fit) * chosen;
	// in twentieths, so a window dragged a pixel wider doesn't re-lay the page
	return std::max(0.25f, std::round(scale * 20.0f) / 20.0f);
}

void UiScale::choose(float c)
{
	chosen = clamped(c);
}

namespace {
float clamped(float chosen)
{
	if (!std::isfinite(chosen))
		throw std::invalid_argument("ui_scale must be a number");
	return std::clamp(chosen, UiScale::MIN_CHOSEN, UiScale::MAX_CHOSEN);
}
} // namespace

} // namespace shell
