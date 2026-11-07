uniform vec3 litCentre;
uniform vec3 litSun;
uniform vec3 litSecond;
uniform float litShare; // the second star's part of the light
uniform float litOn;    // 0: all lit
uniform float litContour; // blocks between contour lines; 0: none

uniform float litPenumbra; // the star's angular radius
uniform vec4 litOcc0;   // what can eclipse it: xyz where, w radius (0: none)
uniform vec4 litOcc1;
uniform vec4 litOcc2;
uniform vec4 litOcc3;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 varWorld;
VARYING_ mediump vec3 varNormal;
VARYING_ highp float varHeight;

// How much steeper slopes are drawn than they are: a mountain is small beside
// a planet, and a map's hillshade exaggerates too
const float RELIEF = 3.0;

// How much of the star a sphere leaves, seen from a point
float eclipse(vec4 o, vec3 at)
{
	if (o.w <= 0.0)
		return 1.0;
	vec3 to = o.xyz - at;
	float along = dot(to, litSun);
	if (along <= 0.0)
		return 1.0;
	float off = length(to - litSun * along);
	float blur = along * litPenumbra;
	float left = smoothstep(abs(o.w - blur), o.w + blur, off);
	// too far off to cover the star: only ever a bite of it
	float most = blur > o.w ? (o.w / blur) * (o.w / blur) : 1.0;
	return 1.0 - (1.0 - left) * most;
}

// Lines where the height crosses a multiple of `step`, a pixel or two wide;
// none where they'd crowd closer than a few pixels
float contour(float step)
{
	float band = varHeight / step;
	float across = fwidth(band);
	float edge = fwidth(floor(band));
	return edge > 0.0 ? 1.0 - smoothstep(0.15, 0.35, across) : 0.0;
}

void main(void)
{
	vec3 up = normalize(varWorld - litCentre);
	// the land's slope, steeper as RELIEF says; walls (and spheres) keep their own
	vec3 n = normalize(varNormal);
	float facing = dot(n, up);
	vec3 ground = facing > 0.5 ? normalize(up + (n - up * facing) * RELIEF) : up;
	// a map's light, from the north-west and 45° up, so relief reads from straight above;
	// flat ground is as lit as it was
	vec3 north = vec3(0.0, 1.0, 0.0) - up * up.y;
	north = dot(north, north) > 1e-6 ? normalize(north) : vec3(1.0, 0.0, 0.0);
	vec3 east = cross(up, north);
	vec3 map = normalize(up + normalize(north - east));
	float hill = clamp(dot(ground, map) / dot(up, map), 0.3, 1.45);
	// as SystemLayer::sphere lights a far body
	float shade = eclipse(litOcc0, varWorld) * eclipse(litOcc1, varWorld) * eclipse(litOcc2, varWorld) * eclipse(litOcc3, varWorld);
	// the star on the slope itself: the ground facing away goes dark first at dusk
	float day = max(dot(ground, litSun), 0.0) * shade * (1.0 - litShare) + max(dot(ground, litSecond), 0.0) * litShare;
	// the night side: only sky-glow on it, deep blue, the land's shapes barely there
	vec3 lit = mix(vec3(1.0), mix(vec3(0.045, 0.06, 0.10), vec3(1.0), day), litOn);
	vec3 colour = varColor.rgb * lit * mix(1.0, hill, mix(0.6, 1.0, 1.0 - litOn));
	if (litContour > 0.0 && varHeight > 0.5) {
		float major = contour(litContour * 5.0), minor = contour(litContour);
		colour *= 1.0 - max(major * 0.55, minor * 0.3);
	}
	gl_FragColor = vec4(colour, 1.0);
}
