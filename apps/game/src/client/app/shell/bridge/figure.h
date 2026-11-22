// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/figure/figure_view.h"

namespace shell {

class Shell;

/*
	The player's figure, drawn natively where the page leaves room for it (FigureView), for the
	whole session:

	  figure.show  { rect: { x, y, width, height, scale } }: standing there, under the page
	  figure.place { rect }: moved there (the page's box moved or grew)
	  figure.hide
*/
class Figures
{
public:
	struct Opts
	{
		Shell &shell;
	};
	explicit Figures(Opts o);
	~Figures();
	Figures(const Figures &) = delete;
	Figures &operator=(const Figures &) = delete;

private:
	Shell &m_shell;
	FigureView m_view; // the main thread's
	int m_underlay = 0;
};

} // namespace shell
