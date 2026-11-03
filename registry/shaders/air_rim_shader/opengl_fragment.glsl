// The camera's at the origin. Columns by how the star stands over the place seen,
// rows by how high the sight line passes (air::rimImage).
uniform sampler2D baseTexture;
uniform vec3 rimCentre;
uniform vec3 rimSun;
uniform float rimGround;
uniform float rimShare;

VARYING_ highp vec3 varWorld;

void main(void)
{
	vec3 ray = normalize(varWorld);
	float along = dot(rimCentre, ray);
	vec3 low = ray * along - rimCentre; // from the centre to the line's nearest point
	float off = length(low);
	float height, fade;
	vec3 place;
	if (off < rimGround) {
		// over the land: only near the edge is there much air along the way
		float t = along - sqrt(rimGround * rimGround - off * off);
		place = normalize(ray * t - rimCentre);
		height = 0.0;
		float edge = 1.0 - max(dot(place, -ray), 0.0);
		fade = edge * edge * edge;
	} else {
		place = low / off;
		height = (off / rimGround - 1.0) / rimShare;
		fade = clamp(1.0 - height, 0.0, 1.0);
	}
	vec3 colour = texture2D(baseTexture, vec2(dot(place, rimSun) * 0.5 + 0.5, height)).rgb * fade;
	// over: colour + what's behind × (1 − cover), as plain alpha blending takes it
	float cover = max(max(colour.r, colour.g), colour.b);
	gl_FragColor = vec4(cover > 0.001 ? colour / cover : vec3(0.0), cover);
}
