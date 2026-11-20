// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/host/pixels.h"
#include "library/trace/trace.h"
#include <algorithm>
#include <array>

namespace shell {

// UNPREMULTIPLY[a][c] = c * 255 / a: a table lookup instead of a division per channel.
static const auto UNPREMULTIPLY = [] {
	std::array<std::array<u8, 256>, 256> table{};
	for (u32 a = 1; a < 256; a++)
		for (u32 c = 0; c < 256; c++)
			table[a][c] = (u8)std::min<u32>(255, c * 255 / a);
	return table;
}();

void Pixels::write(const void *premultiplied, u32 w, u32 h, const std::vector<CefRect> &changed)
{
	std::lock_guard<std::mutex> guard(lock);
	std::vector<CefRect> rects = changed;
	if (w != width || h != height) {
		width = w;
		height = h;
		bgra.assign((size_t)w * h * 4, 0);
		rects = { CefRect(0, 0, (int)w, (int)h) }; // nothing old is reusable
	}

	if (painted.paints++ == 0)
		painted.first = trace::now();
	const u8 *src = static_cast<const u8 *>(premultiplied);
	for (const CefRect &r : rects) {
		u32 x0 = (u32)std::max(0, r.x), y0 = (u32)std::max(0, r.y);
		u32 x1 = std::min(w, (u32)(r.x + r.width)), y1 = std::min(h, (u32)(r.y + r.height));
		if (x1 > x0 && y1 > y0)
			painted.area += (double)(x1 - x0) * (y1 - y0) / ((double)w * h);
		for (u32 y = y0; y < y1; y++) {
			const u8 *s = src + ((size_t)y * w + x0) * 4;
			u8 *d = bgra.data() + ((size_t)y * w + x0) * 4;
			for (u32 x = x0; x < x1; x++, s += 4, d += 4) {
				const auto &row = UNPREMULTIPLY[s[3]];
				d[0] = row[s[0]];
				d[1] = row[s[1]];
				d[2] = row[s[2]];
				d[3] = s[3];
			}
		}
	}
	dirty = true;
}

u8 Pixels::alphaAt(s32 x, s32 y) const
{
	std::lock_guard<std::mutex> guard(lock);
	if (x < 0 || y < 0 || (u32)x >= width || (u32)y >= height)
		return 0;
	return bgra[((size_t)y * width + x) * 4 + 3];
}

} // namespace shell
