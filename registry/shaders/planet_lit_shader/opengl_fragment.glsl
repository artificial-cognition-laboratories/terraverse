uniform vec3 litCentre;
uniform vec3 litSun;
uniform vec3 litSecond;
uniform float litShare; // the second star's part of the light
uniform float litOn;    // 0: all lit

uniform float litPenumbra; // the star's angular radius
uniform vec4 litOcc0;   // what can eclipse it: xyz where, w radius (0: none)
uniform vec4 litOcc1;
uniform vec4 litOcc2;
uniform vec4 litOcc3;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 varWorld;

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

void main(void)
{
	vec3 up = normalize(varWorld - litCentre);
	// as SystemLayer::sphere lights a far body
	float shade = eclipse(litOcc0, varWorld) * eclipse(litOcc1, varWorld) * eclipse(litOcc2, varWorld) * eclipse(litOcc3, varWorld);
	float day = max(dot(up, litSun), 0.0) * shade * (1.0 - litShare) + max(dot(up, litSecond), 0.0) * litShare;
	float lit = mix(1.0, day * 0.92 + 0.08, litOn);
	gl_FragColor = vec4(varColor.rgb * lit, 1.0);
}
