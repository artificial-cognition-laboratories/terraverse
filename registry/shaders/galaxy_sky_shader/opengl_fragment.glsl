// The whole sky's picture: longitude across, latitude down (planetarium/galaxy.cpp).
uniform sampler2D baseTexture;
uniform sampler2D texture1;
uniform float galaxyFade;
uniform float galaxyGrain;

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
	float speck = specks(way, facing, 0.93);
	if (galaxyGrain > 0.0)
		speck = mix(speck, specks(way, facing, 1.6), galaxyGrain);
	colour *= mix(1.0, speck * 3.0, 0.55);
	// long fades on near-black band on 8 bits: a shade of noise, by the screen's pixel
	float grain = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	gl_FragColor = vec4(colour + (grain - 0.5) / 255.0, 1.0);
}
