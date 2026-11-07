// A body's land from its tiles, lit from where its star is (planetarium/lit.h).
uniform mat4 mWorld;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 varWorld;
VARYING_ mediump vec3 varNormal;
VARYING_ highp float varHeight;

void main(void)
{
	varWorld = (mWorld * inVertexPosition).xyz;
	varNormal = (mWorld * vec4(inVertexNormal, 0.0)).xyz;
	// blocks over the sea (far/tiles.cpp onPlanet)
	varHeight = inTexCoord0.x;
	varColor = inVertexColor;
	gl_Position = mWorldViewProj * inVertexPosition;
}
