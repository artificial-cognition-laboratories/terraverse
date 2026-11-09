uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float farBehind;     // 1: the backdrop pass, whose depth is cleared after it
uniform float farCut;        // the near world's range from the eye (scene units): nearer, its blocks are the land
uniform float farShare;      // 0 in the air's haze .. 1 above the air, seeing the planet
uniform highp vec4 farPlace; // the eye: map x, z (blocks), altitude (m), unused
uniform float farAir;        // m: the air's scale height
uniform float farLevel;      // far_view_debug_levels: the tile's level, to tint it by; −1 none

// a hue a level, round the wheel, so neighbouring levels differ
vec3 levelTint(float level)
{
	vec3 k = mod(vec3(0.0, 2.0, 4.0) + level * 1.7, 6.0);
	return clamp(abs(k - 3.0) - 1.0, 0.0, 1.0);
}

VARYING_ lowp vec4 varColor;
VARYING_ highp float varSeen;  // how far off it's seen, scene units
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude;
VARYING_ lowp vec3 varDay;

const float BS = 10.0;              // scene units per block
const float CLEAR = 3.0 / 60000.0;  // per m: the air's haze through clear air, at the sea
const vec3 AIR = vec3(0.55, 0.68, 0.85); // the colour of lit air seen from above

// How much air a straight line from the eye's height to `altitude` passes
// through, per metre of it, against air at the sea.
float airmass(float eye, float altitude)
{
	float rise = altitude - eye;
	float low = exp(-min(eye, altitude) / farAir);
	if (abs(rise) < 1.0)
		return low;
	return low * (1.0 - exp(-abs(rise) / farAir)) * farAir / abs(rise);
}

void main(void)
{
	// within the near world's range its blocks are the land
	// Nearer, the near world's blocks are the land (the backdrop pass draws it all,
	// under them, for where they haven't arrived)
	if (farBehind < 0.5 && varSeen < farCut)
		discard;

	vec4 col = vec4(varColor.rgb, 1.0);
	// (a stand-in, behind the rest: darker)
	if (farLevel >= 0.0)
		col.rgb = mix(col.rgb, levelTint(farLevel), 0.6) * (farBehind > 0.5 ? 0.45 : 1.0);
	float dist = length(eyeVec);

	// the fog only over the last of the view's reach, so it ends in the sky's
	// colour, not a line (high up there's no edge: the planet's all there).
	// The near world's fog, all of the reach from 40% of it, hid land a few
	// kilometres off.
	float clarity = 1.0 - smoothstep(0.75, 1.0, dist / fogDistance);
	clarity = mix(clarity, 1.0, farShare);
	// ... and the air: haze through the air the sight line crosses, as
	// through real clear air at every height, so kilometres read as
	// kilometres (tens of them seen on a clear day) and the air thins as you
	// climb. (It once built over the far view's reach near the ground: half
	// gone 4 km off.)
	float past = max(varSeen - farCut, 0.0) / BS;
	float through = mix(past, dist / BS, farShare);
	float haze = exp(-CLEAR * through * airmass(farPlace.z, max(varAltitude, 0.0)));

	// near the ground the haze is the sky's fog; from above, lit air
	float fogColorMax = max(max(fogColor.r, fogColor.g), fogColor.b);
	if (fogColorMax < 0.0000001) fogColorMax = 1.0;
	vec4 fog = fogColor * pow(fogColor / fogColorMax, vec4(2.0 * clarity * haze));
	vec3 air = mix(fog.rgb, AIR * varDay, farShare);
	col.rgb = mix(fog.rgb, col.rgb, clarity);
	col.rgb = mix(air, col.rgb, haze);
	gl_FragColor = vec4(col.rgb, 1.0);
}
