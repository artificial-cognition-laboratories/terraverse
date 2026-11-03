// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sun.h"

namespace terragen {

fix declination(fix tilt_degrees, fix date)
{
	fix tilt = (fix)(((int64_t)tilt_degrees * PI) / toFix(180));
	return mul(tilt, sin(mul(TWO_PI, date)));
}

fix elevation(fix latitude, fix decl, fix hour)
{
	fix sl, cl, sd, cd;
	sincos(latitude, sl, cl);
	sincos(decl, sd, cd);
	fix up = mul(sl, sd) + mul(mul(cl, cd), cos(mul(TWO_PI, hour - HALF)));
	return HALF_PI - acos(clamp(up, -ONE, ONE));
}

// The sun's half-arc above the horizon, radians: cos h = −tan φ · tan δ,
// clamped at polar day (h = π) and polar night (h = 0).
static fix halfArc(fix sl, fix cl, fix sd, fix cd)
{
	fix num = -mul(sl, sd), den = mul(cl, cd);
	if (den <= 0 || num >= den)
		return num >= 0 ? 0 : PI;
	if (num <= -den)
		return PI;
	return acos(div(num, den));
}

fix dayShare(fix latitude, fix decl)
{
	fix sl, cl, sd, cd;
	sincos(latitude, sl, cl);
	sincos(decl, sd, cd);
	return div(halfArc(sl, cl, sd, cd), PI);
}

fix insolation(fix latitude, fix decl)
{
	fix sl, cl, sd, cd;
	sincos(latitude, sl, cl);
	sincos(decl, sd, cd);
	fix h = halfArc(sl, cl, sd, cd);
	return mul(mul(h, sl), sd) + mul(mul(cl, cd), sin(h));
}

fix annualInsolation(fix latitude, fix tilt_degrees)
{
	// asked over and over of one place (a column, a climate's cell)
	thread_local fix last_latitude = 0, last_tilt = -1, last = 0;
	if (latitude == last_latitude && tilt_degrees == last_tilt)
		return last;
	const int SAMPLES = 16;
	fix sum = 0;
	for (int i = 0; i < SAMPLES; i++) {
		fix date = ratio(2 * i + 1, 2 * SAMPLES);
		sum += insolation(latitude, declination(tilt_degrees, date));
	}
	last_latitude = latitude;
	last_tilt = tilt_degrees;
	return last = sum / SAMPLES;
}

} // namespace terragen
