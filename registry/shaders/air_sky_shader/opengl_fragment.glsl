// Each star's sky from its picture (air::skyImage): round from toward the star,
// and up by the square root of the angle, so the horizon has the most rows.
uniform sampler2D baseTexture;
uniform sampler2D texture1;
uniform vec3 airWay0;
uniform vec3 airWay1;
uniform vec3 airSun0;
uniform vec3 airSun1;
uniform vec2 airDiscs;  // the cosine of each star's radius; above 1: not drawn
uniform vec2 airDips;   // how far below level the drawn planet's and the real air's horizons are
uniform float airSecond; // 1: a second picture

VARYING_ highp vec3 varRay;

const float PI = 3.14159265;

vec2 place(vec3 ray, vec3 way)
{
	// air::realElevation: the picture's horizon where the land's is
	float e = asin(clamp(ray.y, -1.0, 1.0));
	float right = PI / 2.0;
	e = e >= -airDips.x
		? (e + airDips.x) / (right + airDips.x) * (right + airDips.y) - airDips.y
		: -airDips.y + (e + airDips.x) / (right - airDips.x) * (right - airDips.y);
	float angle = e / right;
	float v = 0.5 + 0.5 * sign(angle) * sqrt(abs(angle));
	vec2 a = ray.xz, b = way.xz;
	float la = length(a), lb = length(b);
	float round = la > 1e-5 && lb > 1e-5 ? acos(clamp(dot(a, b) / (la * lb), -1.0, 1.0)) / PI : 0.0;
	return vec2(round, 1.0 - v);
}

void main(void)
{
	vec3 ray = normalize(varRay);
	vec4 sky = vec4(0.0, 0.0, 0.0, 1.0);
	if (airDiscs.x <= 1.0)
		sky = texture2D(baseTexture, place(ray, airWay0));
	if (airSecond > 0.5) {
		vec4 other = texture2D(texture1, place(ray, airWay1));
		sky.rgb += other.rgb;
		sky.a = min(sky.a, other.a);
	}
	vec3 colour = sky.rgb;
	// the discs, too small for the pictures, in what the air's left of their light;
	// down to the drawn planet's horizon, which dips as the eye climbs (the land
	// drawn over them hides the rest: a level cut sliced the sun at a horizon
	// that wasn't there)
	if (asin(clamp(ray.y, -1.0, 1.0)) > -airDips.x - 0.02) {
		float soft = 0.00002;
		colour += airSun0 * smoothstep(airDiscs.x - soft, airDiscs.x + soft, dot(ray, airWay0));
		colour += airSun1 * smoothstep(airDiscs.y - soft, airDiscs.y + soft, dot(ray, airWay1));
	}
	colour = min(colour, vec3(1.0));
	// over: colour + space × alpha, as plain alpha blending takes it
	float cover = max(1.0 - sky.a, max(max(colour.r, colour.g), colour.b));
	gl_FragColor = vec4(cover > 0.001 ? colour / cover : vec3(0.0), cover);
}
