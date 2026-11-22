// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace shell {

/*
	How big the page is drawn: device pixels per CSS pixel. The display's
	density alone leaves a big window's interface small (the page's measure
	is a website's, 1200 CSS pixels, lost in a 2560-wide screen), so it's
	grown with the window until the page sees about REFERENCE CSS pixels each
	way, never shrunk below the density, and the player's own choice (the
	`ui_scale` setting, Ctrl+wheel) multiplies that.
*/
class UiScale
{
public:
	struct Opts
	{
		float density; // the display's own (RenderingEngine::getDisplayDensity)
		float chosen;  // the player's multiplier, 1 by default
	};
	explicit UiScale(Opts o);

	// The scale for a window this many device pixels across and down.
	float of(unsigned width, unsigned height) const;
	void choose(float chosen);
	// Chromium's zoom level for a scale (a factor of 1.2 a level)
	static double zoomLevel(float scale);

	// The CSS size the page is laid out for, before the player's choice
	static constexpr float REFERENCE_WIDTH = 1280, REFERENCE_HEIGHT = 800;
	static constexpr float MIN_CHOSEN = 0.5f, MAX_CHOSEN = 2.0f;

private:
	float density;
	float chosen;
};

} // namespace shell
