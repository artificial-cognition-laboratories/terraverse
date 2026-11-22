// The whole sky's picture: longitude across, latitude down (planetarium/galaxy.cpp).
uniform sampler2D baseTexture;
uniform sampler2D texture1;
uniform float galaxyFade;
uniform float galaxyArt;   // 1: drawn to be pixel art (planetarium/pixels.h): the band alone, its contrast up
uniform float galaxyOpen;  // the eye (planetarium/space/eye.h): 1 dark-adapted, less with the sun in view
uniform float galaxySpecks; // the specks' repeats: about a texel a pixel (galaxy.cpp)

VARYING_ highp vec3 varWay;

float specks(vec3 way, vec3 facing, float size)
{
	return texture2D(texture1, way.yz * size).r * facing.x +
			texture2D(texture1, way.zx * size).r * facing.y +
			texture2D(texture1, way.xy * size).r * facing.z;
}

void main(void)
{
	highp vec3 way = normalize(varWay);
	const float PI = 3.14159265;
	vec2 at = vec2(atan(way.z, way.x) / (2.0 * PI), 0.5 - asin(clamp(way.y, -1.0, 1.0)) / PI);
	vec3 colour = texture2D(baseTexture, at).rgb * galaxyFade;
	// the stars too many to draw: a texture laid over the sky from three sides
	// (one side's flat projection is good where it faces us; no seams, no poles)
	vec3 facing = abs(way);
	facing *= facing;
	facing *= facing;
	facing /= facing.x + facing.y + facing.z;
	float speck = specks(way, facing, galaxySpecks) * 3.0; // 1 its mean, up to 3
	// The eye (planetarium/space/eye.h): adjusted to what's in view, it takes
	// the gas down with it, and the faint stars, but a star it still sees is
	// as bright as ever. Two parts: the band's glow, thin; and its specks,
	// only the brighter ones, the night showing between them.
	const float BLACK = 0.05, GAS = 0.6;
	vec3 gas = max(colour * GAS * galaxyOpen - BLACK, 0.0) / (1.0 - BLACK);
	float cut = mix(2.3, 1.5, galaxyOpen); // the faintest speck still seen
	float seen = max(speck - cut, 0.0) / (3.0 - cut);
	vec3 stars = colour * seen * 2.2;
	colour = gas + stars;
	if (galaxyArt > 0.5) {
		// pixel art: no specks (an art pixel of them is noise; the stars are drawn,
		// placed), and the band's contrast pushed, so its dust lanes read as shapes
		vec3 band = max(texture2D(baseTexture, at).rgb * galaxyFade * galaxyOpen - 0.03, 0.0);
		colour = pow(band * 2.4, vec3(1.45)) * 0.32;
	}
	// the night: the website's own (apps/site variables.scss, $night-sky #04070b)
	colour += vec3(0.016, 0.027, 0.043);
	// long fades on near-black band on 8 bits: a shade of noise, by the screen's pixel
	float grain = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	gl_FragColor = vec4(colour + (grain - 0.5) / 255.0, 1.0);
}
