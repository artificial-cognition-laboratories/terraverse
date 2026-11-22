// The scene as pixel art (planetarium/pixels.h), as the website draws its sky
// (apps/site lib/sky.ts). The scene's drawn whole; each art pixel is made from
// the block of it under it: leaning to the block's brightest, so a star stays
// a point of light, not averaged away, while faint haze between them is. Its
// colour's then stepped to a few shades a channel, two shades meeting through
// a 4×4 ordered dither, never a smooth gradient; the night itself stays flat.
uniform sampler2D baseTexture;
uniform float pixelsCell;   // screen pixels to an art pixel (whole, at most 6: the block read)
uniform float pixelsShades; // shades a channel
uniform vec2 pixelsSize;    // the scene's picture, pixels
uniform vec2 pixelsCorner;  // its lower left, in the window's pixels

float bayer(vec2 at)
{
	// the 4×4 Bayer matrix, 0..15, by an art pixel's place
	vec2 p = mod(floor(at), 4.0);
	float a = mod(p.x, 2.0), b = mod(p.y, 2.0), c = floor(p.x / 2.0), d = floor(p.y / 2.0);
	float small = 2.0 * a * (1.0 - b) + 3.0 * a * b + b * (1.0 - a);
	float large = 2.0 * c * (1.0 - d) + 3.0 * c * d + d * (1.0 - c);
	return 4.0 * small + large;
}

void main(void)
{
	vec2 art = floor((gl_FragCoord.xy - pixelsCorner) / pixelsCell);
	vec2 from = art * pixelsCell;
	vec3 sum = vec3(0.0), most = vec3(0.0);
	float n = 0.0;
	for (int j = 0; j < 6; j++)
		for (int i = 0; i < 6; i++) {
			if (float(i) >= pixelsCell || float(j) >= pixelsCell)
				continue;
			vec3 c = texture2D(baseTexture, (from + vec2(float(i), float(j)) + 0.5) / pixelsSize).rgb;
			sum += c;
			most = max(most, c);
			n += 1.0;
		}
	// The scene drifts under the art pixels, so whatever's dark must change
	// smoothly as it does, or it flickers: the dark is the block's mean (it moves
	// continuously), and only the bright lean to its brightest (a star stays a point)
	vec3 mean = sum / n;
	float peak = max(max(most.r, most.g), most.b);
	// a little lifted: stepped shades read darker than the smooth picture did
	vec3 colour = mix(mean, mix(mean, most, 0.55), smoothstep(0.12, 0.3, peak)) * 1.25;

	const vec3 NIGHT = vec3(0.016, 0.027, 0.043); // the website's, the galaxy's black (galaxy_sky_shader)
	// the dither only where there's light to step through: in the dark it
	// flips a shade back and forth on every slight move
	float lit = max(max(colour.r, colour.g), colour.b);
	float threshold = mix(0.5, (bayer(art) + 0.5) / 16.0, smoothstep(0.03, 0.1, lit));
	float steps = pixelsShades - 1.0;
	// perceptual steps: the dark end gets the finer ones
	vec3 stepped = floor(sqrt(colour) * steps + threshold) / steps;
	stepped *= stepped;
	// the night is one flat colour, as the site's: no dither crawling over empty
	// space, and the faintest stars gone into it, so the bright ones stand out
	gl_FragColor = vec4(mix(NIGHT, stepped, smoothstep(0.06, 0.12, lit)), 1.0); // (from above the night itself: it stays one flat colour)
}
