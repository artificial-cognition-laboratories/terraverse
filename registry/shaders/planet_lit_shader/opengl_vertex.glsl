// A body's land from its tiles, lit from where its star is (planetarium/lit.h).
uniform mat4 mWorld;
// 1: stand-ins (planetarium/lit.h): their depth squeezed into just short of the
// far plane, in order, so they show only where nothing's drawn yet
uniform float litBehind;

VARYING_ lowp vec4 varColor;
VARYING_ lowp float varSkirt; // 1: a skirt, hung below a tile's edge to fill a crack (far/mesh.cpp)
VARYING_ highp vec3 varWorld;
VARYING_ mediump vec3 varNormal;
VARYING_ highp float varHeight;
VARYING_ mediump vec2 varTexCoord;

void main(void)
{
	varWorld = (mWorld * inVertexPosition).xyz;
	varNormal = (mWorld * vec4(inVertexNormal, 0.0)).xyz;
	// blocks over the sea (far/tiles.cpp onPlanet)
	varHeight = inTexCoord0.x;
	// a globe's: where on its texture (planetarium/globe.h)
	varTexCoord = inTexCoord0.xy;
	varColor = inVertexColor;
	varSkirt = inVertexColor.a < 0.5 ? 1.0 : 0.0;
	gl_Position = mWorldViewProj * inVertexPosition;
	if (litBehind > 0.5)
		gl_Position.z = (0.9980 + 0.0019 * clamp(gl_Position.z / gl_Position.w, -1.0, 1.0)) * gl_Position.w;
}
